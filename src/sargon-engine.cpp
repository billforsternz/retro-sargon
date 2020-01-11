/*

  A primitive Windows UCI chess engine interface around the classic program Sargon,
  as presented in the book "Sargon a Z80 Computer Chess Program" by Dan and Kathe
  Spracklen (Hayden Books 1978). Another program in this suite converts the Z80 code
  to working X86 assembly language.
  
  */

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <io.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <algorithm>
#include "util.h"
#include "thc.h"
#include "sargon-interface.h"
#include "sargon-asm-interface.h"
#include "translate.h"
//#define RANDOM_ENGINE

struct NODE
{
    unsigned int level;
    unsigned char from;
    unsigned char to;
    unsigned char flags;
    unsigned char value;
    NODE() : level(0), from(0), to(0), flags(0), value(0) {}
    NODE( unsigned int l, unsigned char f, unsigned char t, unsigned char fs, unsigned char v ) : level(l), from(f), to(t), flags(fs), value(v) {}
};

static std::string logfile_name;
static std::vector< NODE > nodes;

//-- preferences
#define VERSION "1978"
#define ENGINE_NAME "Sargon"

static thc::ChessRules the_position;
static std::vector<thc::Move> the_pv;
static int the_pv_value;
static int the_pv_depth;
static std::vector<thc::Move> the_pv_provisional;
static int the_pv_value_provisional;
static int the_pv_depth_provisional;
bool process( const char *buf );
const char *cmd_uci();
const char *cmd_isready();
const char *cmd_stop();
const char *cmd_go( const char *cmd );
const char *cmd_go_infinite();
void        cmd_multipv( const char *cmd );
const char *cmd_position( const char *cmd );
static bool is_new_game();
static void log( const char *fmt, ... );
static bool RunSargon();
static void AcceptPv();
static void BuildPv();

static HANDLE hSem;
#define BIGBUF 512
#define CMD_BUF_NBR 64
static char cmdline[CMD_BUF_NBR][BIGBUF+2];
static int cmd_put;
static int cmd_get;
static bool signal_stop;
static thc::Move bestmove;

static const char *test_sequence[] =
{
#if 0
    "uci\n",
    "setoption name MultiPV value 4\n",
    "isready\n",
    "position fen 8/8/q2pk3/2p5/8/3N4/8/4K2R w K - 0 1\n",
    "go infinite\n"
#else
    // For debugging subtle timing problem
    "uci\n",
    "isready\n",
    "position fen r2q1rk1/ppp2ppp/2n1bn2/3pp3/1b6/2NPP1P1/PPP1NPBP/R1BQ1RK1 w - - 0 1\n",
    "go infinite\n",
#endif
};

void read_stdin()
{
    bool quit=false;
    while(!quit)
    {
        #if 0
        static int test;
        if( test < sizeof(test_sequence) / sizeof(test_sequence[0]) )
            strcpy( &cmdline[cmd_put][0], test_sequence[test++] );
        else if( NULL == fgets(&cmdline[cmd_put][0],BIGBUF,stdin) )
            quit = true;
        if( !quit )
        #else
        if( NULL == fgets(&cmdline[cmd_put][0],BIGBUF,stdin) )
            quit = true;
        else
        #endif
        {
            if( strchr(&cmdline[cmd_put][0],'\n') )
                *strchr(&cmdline[cmd_put][0],'\n') = '\0';
            if( 0 == _strcmpi(&cmdline[cmd_put][0],"quit") )
            {
                signal_stop = true;
                quit = true;
            }
            else if( 0 == _strcmpi(&cmdline[cmd_put][0],"stop") )
                signal_stop = true;
        }
        cmd_put++;
        cmd_put &= (CMD_BUF_NBR-1);
        ReleaseSemaphore(hSem, 1, 0);
    }
}

void write_stdout()
{
    bool quit=false;
    while(!quit)
    {
        WaitForSingleObject(hSem, INFINITE);
        signal_stop = false;
        const char *cmd = &cmdline[cmd_get][0];
        log( "cmd>%s\n", cmd );     // moved from read_stdin(), not good to use non-reentrant log() in both processes
        quit = process(cmd);
        cmd_get++;
        cmd_get &= (CMD_BUF_NBR-1);
    }
} 

int main( int argc, char *argv[] )
{
    std::string filename_base( argv[0] );
    logfile_name = filename_base + "-log.txt";
    unsigned long tid1;
    hSem = CreateSemaphore(NULL,0,10,NULL);
    HANDLE hThread1 = CreateThread(0,
                            0,
                            (LPTHREAD_START_ROUTINE) read_stdin,
                            0,
                            0,
                            &tid1);
    write_stdout();
    CloseHandle(hSem);
    CloseHandle(hThread1);
    return 0;
}

static jmp_buf jmp_buf_env;
static bool RunSargon()
{
    bool aborted = false;
    int val;
    val = setjmp(jmp_buf_env);
    if( val )
        aborted = true;
    else
    {
        sargon(api_CPTRMV);
        AcceptPv();
    }
    return aborted;
}

// Case insensitive pattern match
// Return NULL if no match
// Return ptr into string beyond matched part of string
// Only if more is true, can the string be longer than the matching part
// So if more is false, and NULL is not returned, the returned ptr always points at trailing '\0'
const char *str_pattern( const char *str, const char *pattern, bool more=false );
const char *str_pattern( const char *str, const char *pattern, bool more )
{
    bool match=true;
    int c, d;
    while( match && *str && *pattern )
    {
        c = *str++;
        d = *pattern++;
        if( c != d )
        {
            match = false;
            if( isascii(c) && isascii(d) && toupper(c)==toupper(d) )
                match = true;
            else if( c=='\t' && d==' ' )
                match = true;
        }
        if( match && (c==' '||c=='\t') )
        {
            while( *str == ' ' )
                str++;
        }
    }
    if( !match )
        str = NULL;
    else if( *pattern )
        str = NULL;
    else if( !more && *str )
        str = NULL;
    if( more && str )
    {
        while( *str == ' ' )
            str++;
    }
    return str;
}

bool process( const char *buf )
{
    bool quit=false;
    const char *parm;
    const char *rsp = NULL;
    if( str_pattern(buf,"quit") )
        quit = true;
    else if( str_pattern(buf,"uci") )
        rsp = cmd_uci();
    else if( str_pattern(buf,"isready") )
        rsp = cmd_isready();
    else if( str_pattern(buf,"stop") )
        rsp = cmd_stop();
    else if( str_pattern(buf,"go infinite") )
        rsp = cmd_go_infinite();
    else if( NULL != (parm=str_pattern(buf,"go",true)) )
        rsp = cmd_go( parm );
    else if( NULL != (parm=str_pattern(buf,"setoption name MultiPV value",true)) )
        cmd_multipv( parm );
    else if( NULL != (parm=str_pattern(buf,"position",true)) )
        rsp = cmd_position( parm );
    if( rsp )
    {
        log( "rsp>%s", rsp );
        fprintf( stdout, "%s", rsp );
        fflush( stdout );
    }
    return quit;
}


const char *cmd_uci()
{
    const char *rsp=
#ifdef RANDOM_ENGINE
    "id name Random Moves Engine\n"
    "id author Bill Forster\n"
#else
    "id name " ENGINE_NAME " " VERSION "\n"
    "id author Dan and Kathe Spracklin, Windows port by Bill Forster\n"
#endif
 // "option name Hash type spin min 1 max 4096 default 32\n"
 // "option name MultiPV type spin default 1 min 1 max 4\n"
    "uciok\n";
    return rsp;
}

const char *cmd_isready()
{
    const char *rsp = "readyok\n";
    return rsp;
}

// Measure elapsed time, nodes    
static unsigned long base_time;
extern int DIAG_make_move_primary;	
static int base_nodes;
static char stop_rsp_buf[128];
const char *cmd_stop()
{
    return stop_rsp_buf;
}

static int MULTIPV=1;
void cmd_multipv( const char *cmd )
{
    MULTIPV = atoi(cmd);
    if( MULTIPV < 1 )
        MULTIPV = 1;
    if( MULTIPV > 4 )
        MULTIPV = 4;
}

void ProgressReport()
{
    int     score_cp = the_pv_value;
    int     depth = the_pv_depth;
    thc::ChessRules ce = the_position;
    int score_overide;
    static char buf[1024];
    static char buf_pv[1024];
    static char buf_score[128];
    bool done=false;
    bool have_move=false;
    thc::Move bestmove_so_far;
    bestmove_so_far.Invalid();
    unsigned long now_time = GetTickCount();	
    int nodes = DIAG_make_move_primary-base_nodes;
    unsigned long elapsed_time = now_time-base_time;
    buf_pv[0] = '\0';
    char *s;
    bool overide = false;
    for( unsigned int i=0; i<the_pv.size(); i++ )
    {
        bool okay;
        thc::Move move=the_pv[i];
        ce.PlayMove( move );
        have_move = true;
        if( i == 0 )
            bestmove_so_far = move;    
        s = strchr(buf_pv,'\0');
        sprintf( s, " %s", move.TerseOut().c_str() );
        thc::TERMINAL score_terminal;
        okay = ce.Evaluate( score_terminal );
        if( okay )
        {
            if( score_terminal == thc::TERMINAL_BCHECKMATE ||
                score_terminal == thc::TERMINAL_WCHECKMATE )
            {
                overide = true;
                score_overide = (i+2)/2;    // 0,1 -> 1; 2,3->2; 4,5->3 etc 
                if( score_terminal == thc::TERMINAL_WCHECKMATE )
                    score_overide = 0-score_overide; //negative if black is winning
            }
            else if( score_terminal == thc::TERMINAL_BSTALEMATE ||
                        score_terminal == thc::TERMINAL_WSTALEMATE )
            {
                overide = true;
                score_overide = 0;
            }
        }
        if( !okay || overide )
            break;
    }    
    if( the_position.white )
    {
        if( overide ) 
        {
            if( score_overide > 0 ) // are we mating ?
                sprintf( buf_score, "mate %d", score_overide );
            else if( score_overide < 0 ) // are me being mated ?
                sprintf( buf_score, "mate -%d", (0-score_overide) );
            else if( score_overide == 0 ) // is it a stalemate draw ?
                sprintf( buf_score, "cp 0" );
        }
        else
        {
            sprintf( buf_score, "cp %d", score_cp );
        }
    }
    else
    {
        if( overide ) 
        {
            if( score_overide < 0 ) // are we mating ?
                sprintf( buf_score, "mate %d", 0-score_overide );
            else if( score_overide > 0 ) // are me being mated ?        
                sprintf( buf_score, "mate -%d", score_overide );
            else if( score_overide == 0 ) // is it a stalemate draw ?
                sprintf( buf_score, "cp 0" );
        }
        else
        {
            sprintf( buf_score, "cp %d", 0-score_cp );
        }
    }
    if( elapsed_time == 0 )
        elapsed_time++;
    if( have_move )
    {
        sprintf( buf, "info depth %d score %s hashfull 0 time %lu nodes %lu nps %lu pv%s\n",
                    depth,
                    buf_score,
                    (unsigned long) elapsed_time,
                    (unsigned long) nodes,
                    1000L * ((unsigned long) nodes / (unsigned long)elapsed_time ),
                    buf_pv );
        fprintf( stdout, buf );
        fflush( stdout );
        log( "rsp>%s", buf );
    }
}

void CalculateNextMove( bool new_game, thc::Move &bestmove, unsigned long ms_budget )
{
    bool aborted = false;
    the_pv.clear();
    the_pv_depth = 0;
    the_pv_value = 0;
    int plymax = 3;
    std::string bestmove_terse;
    unsigned long base = GetTickCount();
    std::vector<unsigned long> elapsedv;
    while( !aborted )
    {
        pokeb(MVEMSG,   0 );
        pokeb(MVEMSG+1, 0 );
        pokeb(MVEMSG+2, 0 );
        pokeb(MVEMSG+3, 0 );
        pokeb(PLYMAX,plymax++);
        sargon(api_INITBD);
        sargon_import_position(the_position);
        int moveno=the_position.full_move_count;
        sargon(api_ROYALT);
        pokeb( KOLOR, the_position.white ? 0 : 0x80 );    // Sargon is side to move
        nodes.clear();
        aborted = RunSargon();
        log( "aborted=%s, the_pv.size()=%d moveno=%d\n", aborted?"true":"false", the_pv.size(), moveno );
        if( !aborted )
            ProgressReport();   
        if( the_pv.size() > 0 )    
        {
            bestmove_terse = the_pv[0].TerseOut();
            thc::TERMINAL score_terminal;
            thc::ChessRules ce = the_position;
            ce.PlayMove(the_pv[0]);
            bool okay = ce.Evaluate( score_terminal );
            if( okay )
            {
                if( score_terminal == thc::TERMINAL_BCHECKMATE ||
                    score_terminal == thc::TERMINAL_WCHECKMATE )
                {
                    break;  // We're done, play the move to checkmate opponent
                }
            }
            unsigned long now = GetTickCount();
            unsigned long elapsed = (now-base);
            log( "elapsed = %lu, ms_budget = %lu\n", elapsed, ms_budget );
            if( elapsed >= ms_budget )
                break;  // We're done, we've spent too much time

            // Elapsed time increases a lot with each additional ply, predict the
            //  elapsed time if we do another ply
            elapsedv.push_back(elapsed);
            unsigned long accum=0, multiplier=0, nsamples=0, prev=0;
            for( unsigned long t: elapsedv )
            {
                if( t>prev && prev>0 )
                {
                    multiplier = t/prev;
                    accum += multiplier;
                    nsamples++;
                }
                prev = t;
            }
            if( nsamples > 0 )
            {
                unsigned long avg = accum/nsamples;
                unsigned long predict = elapsed * avg;
                log( "avg = %lu, predict = %lu, ms_budget/2 = %lu\n", avg, predict, ms_budget/2 );
                if( predict > ms_budget/2 )
                    break;  // We're done, we predict we'll spend almost all our time
            }
        }
        else
        {
            // maybe book move
            bestmove_terse = sargon_export_move(BESTM);
            break;
        }
    }
    bool have_move = bestmove.TerseIn( &the_position, bestmove_terse.c_str() );
    if( !have_move )
        log( "Sargon doesn't find move - %s\n%s", bestmove_terse.c_str(), the_position.ToDebugStr().c_str() );
}

const char *cmd_go( const char *cmd )
{
    stop_rsp_buf[0] = '\0';
    static char buf[128];

    // Work out our time and increment
    // eg cmd ="wtime 30000 btime 30000 winc 0 binc 0"
    const char *stime;
    const char *sinc;
    int ms_time   = 0;
    int ms_inc    = 0;
    int ms_budget = 0;
    if( the_position.white )
    {
        stime = "wtime";
        sinc  = "winc";
    }
    else
    {
        stime = "btime";
        sinc  = "binc";
    }
    const char *q = strstr(cmd,stime);
    if( q )
    {
        q += 5;
        while( *q == ' ' )
            q++;
        ms_time = atoi(q);
    }
    q = strstr(cmd,sinc);
    if( q )
    {
        q += 4;
        while( *q == ' ' )
            q++;
        ms_inc = atoi(q);
    }

    // Allocate an amount of time based on how long we've got
    log( "Input ms_time=%d, ms_inc=%d\n", ms_time, ms_inc );
    if( ms_time == 0 )
        ms_budget = 0;
    else if( ms_inc == 0 )
        ms_budget = ms_time/40;
    else if( ms_time > 100*ms_inc )
        ms_budget = ms_time/30;
    else if( ms_time > 50*ms_inc )
        ms_budget = ms_time/20;
    else if( ms_time > 10*ms_inc )
        ms_budget = ms_time/10;
    else if( ms_time > ms_inc )
        ms_budget = ms_time/5;
    else
        ms_budget = ms_time/2;

    // When time gets really short get serious
    if( ms_budget < 50 )
        ms_budget = 0;
    log( "Output ms_budget = %d\n", ms_budget );
    thc::Move bestmove;
    bool new_game = is_new_game();
    CalculateNextMove( new_game, bestmove, ms_budget );
    ProgressReport();
    sprintf( buf, "bestmove %s\n", bestmove.TerseOut().c_str() );
    return buf;
}

const char *cmd_go_infinite()
{
    int plymax=3;
    bool aborted = false;
    the_pv.clear();
    the_pv_depth = 0;
    the_pv_value = 0;
    while( !aborted )
    {
        pokeb(MVEMSG,   0 );
        pokeb(MVEMSG+1, 0 );
        pokeb(MVEMSG+2, 0 );
        pokeb(MVEMSG+3, 0 );
        pokeb(PLYMAX,plymax++);
        sargon(api_INITBD);
        sargon_import_position(the_position,true);  // note avoid_book = true
        thc::ChessRules cr = the_position;
        sargon(api_ROYALT);
        pokeb( KOLOR, the_position.white ? 0 : 0x80 );    // Sargon is side to move
        nodes.clear();
        aborted = RunSargon();
        if( !aborted )
            ProgressReport();   
    }
    if( aborted && the_pv.size() > 0 )
    {
        unsigned long now_time = GetTickCount();	
        int nodes = DIAG_make_move_primary-base_nodes;
        unsigned long elapsed_time = now_time-base_time;
        if( elapsed_time == 0 )
            elapsed_time++;
        sprintf( stop_rsp_buf, "info time %lu nodes %lu nps %lu\n" 
                               "bestmove %s\n",
                               (unsigned long) elapsed_time,
                               (unsigned long) nodes,
                               1000L * ((unsigned long) nodes / (unsigned long)elapsed_time ),
                               the_pv[0].TerseOut().c_str() ); 
    }
    return NULL;
}

static bool different_game;
static bool is_new_game()
{
    return different_game;
}

const char *cmd_position( const char *cmd )
{
    static thc::ChessPosition prev_position;
    static thc::Move last_move, last_move_but_one;
    different_game = true;
    log( "cmd_position(): Setting different_game = true\n" );
    thc::ChessEngine tmp;
    the_position = tmp;    //init
    const char *s, *parm;
    bool look_for_moves = false;
    if( NULL != (parm=str_pattern(cmd,"fen",true)) )
    {
        the_position.Forsyth(parm);
        look_for_moves = true;
    }
    else if( NULL != (parm=str_pattern(cmd,"startpos",true)) )
    {
        thc::ChessEngine tmp;
        the_position = tmp;    //init
        look_for_moves = true;
    }

    // Previous position
    thc::ChessRules temp=prev_position;

    // Previous position updated
    prev_position = the_position;
    if( look_for_moves )
    {
        s = strstr(parm,"moves ");
        if( s )
        {
            last_move_but_one.Invalid();
            last_move.Invalid();
            s = s+5;
            for(;;)
            {
                while( *s == ' ' )
                    s++;
                if( *s == '\0' )
                    break;
                thc::Move move;
                bool okay = move.TerseIn(&the_position,s);
                s += 5;
                if( !okay )
                    break;
                the_position.PlayMove( move );
                last_move_but_one = last_move;
                last_move         = move;
            }

            // Previous position updated with latest moves
            prev_position = the_position;

            // Maybe this latest position is the old one with two new moves ?
            if( last_move_but_one.Valid() && last_move.Valid() )
            {
                temp.PlayMove( last_move_but_one );
                temp.PlayMove( last_move );
                if( prev_position == temp )
                {
                    // Yes it is! so we are still playing the same game
                    different_game = false;
                    log( "cmd_position(): Setting different_game = false %s\n", the_position.ToDebugStr().c_str() );
                }
            }
        }
    }
    return NULL;
}

static void log( const char *fmt, ... )
{
	va_list args;
	va_start( args, fmt );
    static FILE *file_log;
    static bool first=true;
    static char buf[1024];
    if( file_log == 0 )
        file_log = fopen( logfile_name.c_str(), first? "wt" : "at" );
    first = false;
    if( file_log )
    {
        time_t t = time(NULL);
        struct tm *ptm = localtime(&t);
        const char *s = asctime(ptm);
        strcpy( buf, s );
        char *p = strchr(buf,'\n');
        if( p )
            *p = '\0';
        fputs(buf,file_log);
        buf[0] = ':';
        buf[1] = ' ';
        vsnprintf( buf+2, sizeof(buf)-4, fmt, args ); 
        fputs(buf,file_log);
        fclose(file_log);
        file_log = 0;
    }
    va_end(args);
}

static void AcceptPv()
{
    the_pv         = the_pv_provisional;
    the_pv_value   = the_pv_value_provisional;
    the_pv_depth   = the_pv_depth_provisional;
}

static void BuildPv()
{
    the_pv_provisional.clear();
    std::vector<NODE> nodes_pv;
    int nbr = nodes.size();
    int target = 1;
    int plymax = peekb(PLYMAX);
    for( int i=nbr-1; i>=0; i-- )
    {
        NODE *p = &nodes[i];
        if( p->level == target )
        {
            nodes_pv.push_back( *p );
            double fvalue = sargon_export_value(p->value);
            // log( "level=%d, from=%s, to=%s value=%d/%.1f\n", p->level, algebraic(p->from).c_str(), algebraic(p->to).c_str(), p->value, fvalue );
            if( target == plymax )
                break;
            target++;
        }
    }
    thc::ChessRules cr = the_position;
    nbr = nodes_pv.size();
    bool ok = true;
    for( int i=0; ok && i<nbr; i++ )
    {
        NODE *p = &nodes_pv[i];
        thc::Square src;
        ok = sargon_export_square( p->from, src );
        if( ok )
        {
            thc::Square dst;
            ok = sargon_export_square( p->to, dst );
            if( ok )
            {
                char buf[5];
                buf[0] = thc::get_file(src);
                buf[1] = thc::get_rank(src);
                buf[2] = thc::get_file(dst);
                buf[3] = thc::get_rank(dst);
                buf[4] = '\0';
                thc::Move mv;
                mv.TerseIn( &cr, buf );
                cr.PlayMove(mv);
                the_pv_provisional.push_back(mv);
            }
        }
    }
    the_pv_depth_provisional = plymax;
    double fvalue = sargon_export_value( nodes_pv[nbr-1].value );

    // Sargon's values are negated at alternate levels, transforming minimax to maximax.
    //  If White to move, maximise conventional values at level 0,2,4
    //  If Black to move, maximise negated values at level 0,2,4
    bool odd = ((nbr-1)%2 == 1);
    bool negate = the_position.WhiteToPlay() ? odd : !odd;
    double centipawns = (negate ? -100.0 : 100.0) * fvalue;
    the_pv_value_provisional = static_cast<int>(centipawns);
}


extern "C" {
    void callback( uint32_t reg_edi, uint32_t reg_esi, uint32_t reg_ebp, uint32_t reg_esp,
                   uint32_t reg_ebx, uint32_t reg_edx, uint32_t reg_ecx, uint32_t reg_eax,
                   uint32_t reg_eflags )
    {
        uint32_t *sp = &reg_edi;
        sp--;

        // expecting code at return address to be 0xeb = 2 byte opcode, (0xeb + 8 bit relative jump),
        uint32_t ret_addr = *sp;
        const unsigned char *code = (const unsigned char *)ret_addr;
        const char *msg = (const char *)(code+2);   // ASCIIZ text should come after that

        if( 0 == strcmp(msg,"Yes! Best move") )
        {
            unsigned int p      = peekw(MLPTRJ);
            unsigned int level  = peekb(NPLY);
            unsigned char from  = peekb(p+2);
            unsigned char to    = peekb(p+3);
            unsigned char flags = peekb(p+4);
            unsigned char value = peekb(p+5);
            NODE n(level,from,to,flags,value);
            nodes.push_back(n);
            if( level == 1 )
            {
                BuildPv();
                nodes.clear();
            }
        }

        // Abort RunSargon() if signalled by signal_stop being set
        if( signal_stop )
        {
            longjmp( jmp_buf_env, 1 );
        }
    }
};

