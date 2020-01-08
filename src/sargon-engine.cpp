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

static HANDLE hSem;
#define BIGBUF 8190
static char cmdline[8][BIGBUF+2];
static int cmd_put;
static int cmd_get;
static bool gbl_stop;
static thc::Move bestmove;

/*
setoption name MultiPV value 4
isready
position fen 8/8/q2pk3/2p5/8/3N4/8/4K2R w K - 0 1
go infinite


*/

void read_stdin()
{
    bool quit=false;
    while(!quit)
    {
        #if 0
        static int test;
        if( test == 0 )
        {
            strcpy( &cmdline[cmd_put][0], "uci\n" );
            test++;
        }
        else if( test == 1 )
        {
            strcpy( &cmdline[cmd_put][0], "setoption name MultiPV value 4\n" );
            test++;
        }
        else if( test == 2 )
        {
            strcpy( &cmdline[cmd_put][0], "isready\n" );
            test++;
        }
        else if( test == 3 )
        {
            strcpy( &cmdline[cmd_put][0], "position fen 8/8/q2pk3/2p5/8/3N4/8/4K2R w K - 0 1\n" );
            test++;
        }
        else if( test == 4 )
        {
            strcpy( &cmdline[cmd_put][0], "go infinite\n" );
            test++;
        }
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
            log( "cmd>%s\n", &cmdline[cmd_put][0] );
            if( 0 == _strcmpi(&cmdline[cmd_put][0],"quit") )
            {
                gbl_stop = true;
                quit = true;
            }
            else if( 0 == _strcmpi(&cmdline[cmd_put][0],"stop") )
                gbl_stop = true;
        }
        ReleaseSemaphore(hSem, 1, 0);
        cmd_put++;
        cmd_put &= 7;
    }
}

void write_stdout()
{
    bool quit=false;
    while(!quit)
    {
        WaitForSingleObject(hSem, INFINITE);
        quit = process(&cmdline[cmd_get][0]);
        cmd_get++;
        cmd_get &= 7;
    }
} 

static FILE *file_log;
int main( int argc, char *argv[] )
{
    std::string filename_base( argv[0] );
    logfile_name = filename_base + "-log.txt";
    unsigned long tid1;
    HANDLE hThread1;
    HANDLE handle = (HANDLE)_get_osfhandle(_fileno(stdin));
    int last=34;
    hSem = CreateSemaphore(NULL,0,10,NULL);
    hThread1 = CreateThread(0,
                            0,
                            (LPTHREAD_START_ROUTINE) read_stdin,
                            0,
                            0,
                            &tid1);
    write_stdout();
    CloseHandle(hSem);
    CloseHandle(hThread1);
    if( file_log )
        fclose(file_log);
    return 0;
}

static jmp_buf jmp_buf_env;
static bool RunSargon()
{
    bool aborted = false;
    int val;
    gbl_stop = false;
    val = setjmp(jmp_buf_env);
    if( val )
        aborted = true;
    else
        sargon(api_CPTRMV);
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

void ProgressReport
(
    bool    init,
    std::vector<thc::Move> &pv,
    int     score_cp,
    int     depth
)

{
    static thc::ChessPosition pos;
    if(init)
    {
        base_time  = GetTickCount();
        base_nodes = DIAG_make_move_primary;
    }
    else
    {
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
        for( unsigned int i=0; i<pv.size(); i++ )
        {
            bool okay;
            thc::Move move=pv[i];
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
        if( pos.white )
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
}

bool CalculateNextMove( thc::ChessRules &cr, bool new_game, std::vector<thc::Move> &pv, thc::Move &bestmove, int &score_cp,
                        unsigned long ms_time,
                        unsigned long ms_budget,
                        int &depth )
{
    bool have_move = false;
    pokeb(MVEMSG,   0 );
    pokeb(MVEMSG+1, 0 );
    pokeb(MVEMSG+2, 0 );
    pokeb(MVEMSG+3, 0 );
    pokeb(PLYMAX,5);
    sargon(api_INITBD);
    sargon_import_position(cr);
    sargon(api_ROYALT);
    pokeb( KOLOR, cr.white ? 0 : 0x80 );    // Sargon is side to move
    thc::Move mv;
#ifdef  RANDOM_ENGINE
    std::vector<thc::Move> moves;
    cr.GenLegalMoveList( moves );
    int idx = rand() % moves.size();
    mv = moves[idx];
    have_move = true;
#else
    nodes.clear();
    RunSargon();
    thc::ChessRules cr_after;
    sargon_export_position(cr_after);
    std::string terse = sargon_export_move(BESTM);
    have_move = mv.TerseIn( &cr, terse.c_str() );
    if( !have_move )
    {
        log( "Sargon doesn't find move - %s\n%s", terse.c_str(), cr.ToDebugStr().c_str() );
        log( "(After)\n%s",cr_after.ToDebugStr().c_str() );
    }
#endif
    bestmove = mv;
    return have_move;
}

const char *cmd_go( const char *cmd )
{
    stop_rsp_buf[0] = '\0';
    int depth=5;
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
    log( "Output ms_budget%d\n", ms_budget );
    thc::Move bestmove;
    int score_cp=0;
    bestmove.Invalid();
    bool new_game = is_new_game();
    ProgressReport
    (
        true,
        the_pv,
        0,
        0
    );
    bool have_move = CalculateNextMove( the_position, new_game, the_pv, bestmove, score_cp, ms_time, ms_budget, depth );

/*
    // Public interface to version for repitition avoidance
    bool CalculateNextMove( vector<thc::Move> &pv, Move &bestmove, int &score_cp,
                            unsigned long ms_time,
                            int &depth, unsigned long &nodes  );
 */

    ProgressReport
    (
        false,
        the_pv,
        score_cp,
        depth
    );

    sprintf( buf, "bestmove %s\n", bestmove.TerseOut().c_str() );
    return buf;
}

const char *cmd_go_infinite()
{
    int plymax=3;
    bool aborted = false;
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
                    log( "cmd_position(): Setting different_game = false\n" );
                }
            }
        }
    }
    return NULL;
}

static void log( const char *fmt, ... )
{
    if( file_log == 0 )
        file_log = fopen( logfile_name.c_str(), "wt" );
    if( file_log )
    {
        char buf[1024];
        time_t t = time(NULL);
        struct tm *ptm = localtime(&t);
        const char *s = asctime(ptm);
        strcpy( buf, s );
        if( strchr(buf,'\n') )
            *strchr(buf,'\n') = '\0';
        fputs(buf,file_log);
        fputs(": ",file_log);
	    va_list args;
	    va_start( args, fmt );
        char *p = buf;
        vsnprintf( p, sizeof(buf)-2-(p-buf), fmt, args ); 
        fputs(buf,file_log);
        va_end(args);
    }
}

void ShowPv()
{
    ProgressReport
    (
        false,
        the_pv,
        the_pv_value,
        the_pv_depth
    );
}


void BuildPv()
{
    the_pv.clear();
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
            log( "level=%d, from=%s, to=%s value=%d/%.1f\n", p->level, algebraic(p->from).c_str(), algebraic(p->to).c_str(), p->value, fvalue );
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
                the_pv.push_back(mv);
            }
        }
    }
    the_pv_depth = plymax;
    double fvalue = sargon_export_value( nodes_pv[nbr-1].value );

    // Sargon's values are negated at alternate levels, transforming minimax to maximax.
    //  If White to move, maximise conventional values at level 0,2,4
    //  If Black to move, maximise negated values at level 0,2,4
    bool odd = ((nbr-1)%2 == 1);
    bool negate = odd; //(the_position.WhiteToPlay() ? odd : !odd );
    double centipawns = (negate ? -100.0 : 100.0) * fvalue;
    the_pv_value = static_cast<int>(centipawns);
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
            static int previous_plymax;
            int plymax = peekb(PLYMAX);
            if( plymax != previous_plymax )
                ShowPv();
            previous_plymax = plymax;
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

        // Abort RunSargon() if signalled by gbl_stop being set
        if( gbl_stop )
        {
            longjmp( jmp_buf_env, 1 );
        }
    }
};

