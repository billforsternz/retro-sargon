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


struct NODE2
{
    unsigned int level;
    unsigned char from;
    unsigned char to;
    unsigned char flags;
    unsigned char value;
    NODE2() : level(0), from(0), to(0), flags(0), value(0) {}
    NODE2( unsigned int l, unsigned char f, unsigned char t, unsigned char fs, unsigned char v ) : level(l), from(f), to(t), flags(fs), value(v) {}
};

static NODE2 pv[10];
static std::vector< NODE2 > nodes;

extern "C" {
    void callback( uint32_t edi, uint32_t esi, uint32_t ebp, uint32_t esp,
                   uint32_t ebx, uint32_t edx, uint32_t ecx, uint32_t eax,
                   uint32_t eflags )
    {
        uint32_t *sp = &edi;
        sp--;
        uint32_t ret_addr = *sp;
        const unsigned char *code = (const unsigned char *)ret_addr;

        // expecting 0xeb = 2 byte opcode, (0xeb + 8 bit relative jump),
        //  if others skip over 4 operand bytes
        const char *msg = ( code[0]==0xeb ? (char *)(code+2) : (char *)(code+6) );

        if( std::string(msg) == "After FNDMOV()" )
        {
            printf( "After FNDMOV()\n" );
        }
        return; // return now to disable minimax tracing experiment

        // For purposes of minimax tracing experiment, try to figure out
        //  best move calculation
        if( std::string(msg) == "Alpha beta cutoff?" )
        {
            unsigned int al  = eax&0xff;
            unsigned int bx  = ebx&0xffff;
            unsigned int val = peekb(bx);
            bool jmp = (al < val);
            if( jmp )
            {
                printf( "Ply level %d\n", peekb(NPLY));
                printf( "Alpha beta cutoff? Yes if move value=%d/%.1f < 2 lower ply value=%d/%.1f, ",
                    al,  sargon_value_export(al),
                    val, sargon_value_export(val) );
                printf( "So %s\n", jmp?"yes":"no" );
            }
        }
        else if( std::string(msg) == "No. Best move?" )
        {
            unsigned int al  = eax&0xff;
            unsigned int bx  = ebx&0xffff;
            unsigned int val = peekb(bx);
            bool jmp = (al < val);
            if( !jmp )
            {
                printf( "Ply level %d\n", peekb(NPLY));
                printf( "Best move? No if move value=%d/%.1f < 1 lower ply value=%d/%.1f, ",
                    al,  sargon_value_export(al),
                    val, sargon_value_export(val) );
                printf( "So %s\n", jmp?"no":"yes" );
            }
        }
        else if( std::string(msg) == "Yes! Best move" )
        {
            static int best_move_count;
            unsigned int p      = peekw(MLPTRJ);
            unsigned int level  = peekb(NPLY);
            unsigned char from  = peekb(p+2);
            unsigned char to    = peekb(p+3);
            unsigned char flags = peekb(p+4);
            unsigned char value = peekb(p+5);
            NODE2 n(level,from,to,flags,value);
            nodes.push_back(n);
            printf( "Best move found: %s%s (%d)\n", algebraic(from).c_str(), algebraic(to).c_str(), ++best_move_count );
            //diagnostics();
        }
    }
};

void dbg_ptrs()
{
    printf( "BESTM  = %04x\n", peekw(BESTM) );
    printf( "MLPTRI = %04x\n", peekw(MLPTRI) );
    printf( "MLPTRJ = %04x\n", peekw(MLPTRJ) );
    printf( "MLLST  = %04x\n", peekw(MLLST) );
    printf( "MLNXT  = %04x\n", peekw(MLNXT) );
}

void dbg_score()
{
    printf( "SCORE[0] = %d / %f\n", peekb(SCORE),   sargon_value_export(peekb(SCORE))   );
    printf( "SCORE[1] = %d / %f\n", peekb(SCORE+1), sargon_value_export(peekb(SCORE+1)) );
    printf( "SCORE[2] = %d / %f\n", peekb(SCORE+2), sargon_value_export(peekb(SCORE+2)) );
    printf( "SCORE[3] = %d / %f\n", peekb(SCORE+3), sargon_value_export(peekb(SCORE+3)) );
    printf( "SCORE[4] = %d / %f\n", peekb(SCORE+4), sargon_value_export(peekb(SCORE+4)) );
}

void dbg_plyix()
{
    printf( "PLYIX: " );
    unsigned int p = PLYIX;
    for( int i=0; i<20; i++ )
    {
        unsigned int x =peekw(p);
        p += 2;
        if( x )
            printf( "%04x", x );
        else
            printf( "0" );
        printf( "%s",  i+1<20 ? " " : "\n" );
    }
}

void diagnostics()
{
    dbg_ptrs();
    dbg_plyix();
    dbg_score();
    const char *s="";
    unsigned int p=0;
    for( int k=-1; k<24; k++ )
    {
        switch( k )
        {
            case -1: p = peekw(BESTM);     s = "BESTM";        break;
            case 0:  p = peekw(MLPTRI);    s = "MLPTRI";       break;
            case 1:  p = peekw(MLPTRJ);    s = "MLPTRJ";       break;
            case 2:  p = peekw(MLLST);     s = "MLLST";        break;
            case 3:  p = peekw(MLNXT);     s = "MLNXT";        break;
            case 4:  p = peekw(PLYIX);     s = "PLYIX[0]";     break;
            case 5:  p = peekw(PLYIX+2 );  s = "PLYIX[2]";     break;
            case 6:  p = peekw(PLYIX+4 );  s = "PLYIX[4]";     break;
            case 7:  p = peekw(PLYIX+6 );  s = "PLYIX[6]";     break;
            case 8:  p = peekw(PLYIX+8 );  s = "PLYIX[8]";     break;
            case 9:  p = peekw(PLYIX+10);  s = "PLYIX[10]";    break;
            case 10: p = peekw(PLYIX+12);  s = "PLYIX[12]";    break;
            case 11: p = peekw(PLYIX+14);  s = "PLYIX[14]";    break;
            case 12: p = peekw(PLYIX+16);  s = "PLYIX[16]";    break;
            case 13: p = peekw(PLYIX+18);  s = "PLYIX[18]";    break;
            case 14: p = peekw(PLYIX+20);  s = "PLYIX[20]";    break;
            case 15: p = peekw(PLYIX+22);  s = "PLYIX[22]";    break;
            case 16: p = peekw(PLYIX+24);  s = "PLYIX[24]";    break;
            case 17: p = peekw(PLYIX+26);  s = "PLYIX[26]";    break;
            case 18: p = peekw(PLYIX+28);  s = "PLYIX[28]";    break;
            case 19: p = peekw(PLYIX+30);  s = "PLYIX[30]";    break;
            case 20: p = peekw(PLYIX+32);  s = "PLYIX[32]";    break;
            case 21: p = peekw(PLYIX+34);  s = "PLYIX[34]";    break;
            case 22: p = peekw(PLYIX+36);  s = "PLYIX[36]";    break;
            case 23: p = peekw(PLYIX+38);  s = "PLYIX[38]";    break;
        }
        if( p )
        {
            printf( "%9s: ", s );
            for( int i=0; i<50 && (p); i++ )
            {
                unsigned char from  = peekb(p+2);
                unsigned char to    = peekb(p+3);
                unsigned char flags = peekb(p+4);
                unsigned char value = peekb(p+5);
                double fvalue = sargon_value_export(value);
                if( i > 0 )
                    printf( "%9s: ", " " );
                if( p < 0x400 )
                    printf( "link=0x%04x\n", peekw(p) );
                else
                    printf( "link=0x%04x, from=%s, to=%s flags=0x%02x value=%d/%.1f\n", peekw(p), algebraic(from).c_str(), algebraic(to).c_str(), flags, value, fvalue );
                p = peekw(p);
            }
        }
    }
}

//-- preferences
#define VERSION "1978"
#define ENGINE_NAME "Sargon"

static thc::ChessRules the_position;
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

static HANDLE hSem;
#define BIGBUF 8190
static char cmdline[8][BIGBUF+2];
static int cmd_put;
static int cmd_get;
extern bool gbl_stop;
static thc::Move bestmove;

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
            strcpy( &cmdline[cmd_put][0], "position fen r1b2rk1/1p1n1ppp/pBqp4/3Np1b1/2B1P3/1Q3P2/PPP3PP/2KR3R w - - 8 16\n" );
            test++;
        }
        else if( test == 2 )
        {
            strcpy( &cmdline[cmd_put][0], "go wtime 873260 btime 711020 movestogo 25\n" );
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
    "id name " ENGINE_NAME " " VERSION "\n"
    "id author Dan and Kathe Spracklin, Windows port by Bill Forster\n"
 // "option name Hash type spin min 1 max 4096 default 32\n"
//  "option name MultiPV type spin default 1 min 1 max 4\n"
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

#if 0
void ReportOnProgress
(
    bool    init,
    int     multipv,
    std::vector<thc::Move> &pv,
    int     score_cp,
    int     depth
)

{
    static thc::ChessPosition pos;
    if( init )
    {
        pos = the_position;
        base_time  = GetTickCount();
        base_nodes = DIAG_make_move_primary;
    }
    else
    {
        thc::ChessRules ce = pos;
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
            if( multipv )
                sprintf( buf, "info multipv %d depth %d score %s hashfull 0 time %lu nodes %lu nps %lu pv%s\n",
                            multipv,
                            depth+1,
                            buf_score,
                            (unsigned long) elapsed_time,
                            (unsigned long) nodes,
                            1000L * ((unsigned long) nodes / (unsigned long)elapsed_time ),
                            buf_pv );
            else
                sprintf( buf, "info depth %d score %s hashfull 0 time %lu nodes %lu nps %lu pv%s\n",
                            depth+1,
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
#endif

bool CalculateNextMove( thc::ChessRules &cr, bool new_game, std::vector<thc::Move> &pv, thc::Move &bestmove, int &score_cp,
                        unsigned long ms_time,
                        unsigned long ms_budget,
                        int balance,
                        int &depth )
{
    bool have_move = false;
    char buf[5];
    pokeb(MVEMSG,   0 );
    pokeb(MVEMSG+1, 0 );
    pokeb(MVEMSG+2, 0 );
    pokeb(MVEMSG+3, 0 );
    if( cr.white )
    {
        pokeb(COLOR,0);
        pokeb(KOLOR,0);
    }
    else
    {
        pokeb(COLOR,0x80);
        pokeb(KOLOR,0x80);
    }
    pokeb(PLYMAX,3);
    nodes.clear();
    sargon(api_INITBD);
    sargon_position_import(cr);
    sargon(api_ROYALT);
    pokeb(MOVENO,3);    // Move number is 1 at at start, add 2 to avoid book move
    sargon(api_CPTRMV);
    thc::ChessRules cr_after;
    sargon_position_export(cr_after);
    memcpy( buf, peek(MVEMSG), 4 );
    buf[4] = '\0';
    bool trigger = false;
    if( std::string(buf) == "OO" )
    {
        strcpy(buf,cr.white?"e1g1":"e8g8");
        trigger = true;
    }
    else if( std::string(buf) == "OOO" )
    {
        strcpy(buf,cr.white?"e1c1":"e8c8");
        trigger = true;
    }
    else if( std::string(buf) == "EP" )
    {
        trigger = true;
        int target = cr.enpassant_target;
        int src=0;
        if( 0<=target && target<64 && cr_after.squares[target] == (cr.white?'P':'p') )
        {
            if( cr.white )
            {
                int src1 = target + 7;
                int src2 = target + 9;
                if( cr.squares[src1]=='P' && cr_after.squares[src1]!='P' )
                    src = src1;
                else
                    src = src2;
            }
            else
            {
                int src1 = target - 7;
                int src2 = target - 9;
                if( cr.squares[src1]=='p' && cr_after.squares[src1]!='p' )
                    src = src1;
                else
                    src = src2;
            }
        }
#define FILE(sq)    ( (char) (  ((sq)&0x07) + 'a' ) )           // eg c5->'c'
#define RANK(sq)    ( (char) (  '8' - (((sq)>>3) & 0x07) ) )    // eg c5->'5'
        buf[0] = FILE(src);
        buf[1] = RANK(src);
        buf[2] = FILE(target);
        buf[3] = RANK(target);
    }
    thc::Move mv;
    have_move = mv.TerseIn( &cr, buf );
    if( !have_move || trigger )
    {
        log( "%s - %s\n%s", have_move?"Castling or En-Passant":"Sargon doesn't find move", buf, cr.ToDebugStr().c_str() );
        log( "(After)\n%s",cr_after.ToDebugStr().c_str() );
    }
    bestmove = mv;
    return have_move;
}

const char *cmd_go( const char *cmd )
{
    stop_rsp_buf[0] = '\0';
    int depth=5;
    static char buf[128];
    #define BALANCE 4

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
    std::vector<thc::Move> pv;
    bool new_game = is_new_game();
    bool have_move = CalculateNextMove( the_position, new_game, pv, bestmove, score_cp, ms_time, ms_budget, BALANCE, depth );

/*
    // Public interface to version for repitition avoidance
    bool CalculateNextMove( vector<thc::Move> &pv, Move &bestmove, int &score_cp,
                            unsigned long ms_time, int balance,
                            int &depth, unsigned long &nodes  );
 */

/*  // Commented out - let the function report its own progress
    ReportOnProgress
    (
        false,
        1,
        pv,
        score_cp,
        depth
    );  */

    sprintf( buf, "bestmove %s\n", bestmove.TerseOut().c_str() );
    return buf;
}

const char *cmd_go_infinite()
{
#if 0
    stop_rsp_buf[0] = '\0';
    static char buf[1024];
    #define BALANCE 4
    thc::ChessPosition pos = the_position;
    bool done=false;
    thc::Move bestmove_so_far;
    bestmove_so_far.Invalid();
    std::vector<thc::Move> pv;
    ReportOnProgress
    (
        true,
        1,
        pv,
        0,
        0
    );
    for( int depth=0; !done && depth<20; depth++ )
    {
        for( int multi=1; multi<=MULTIPV; multi++ )
        {
            the_position = pos;
            int score;
            bestmove.Invalid();
            bool have_move = the_position.CalculateNextMove( score, bestmove, BALANCE, depth, multi==1 );

            /*
                // A version for Multi-PV mode, called repeatedly, removes best move from
                //  movelist each time
                bool CalculateNextMove( int &score, thc::Move &move, int balance, int depth, bool first );
            */

            if( gbl_stop || (multi==1 && !have_move) )
            {
                done = true;
                break;
            }
            if( !have_move )
                break;
            if( multi==1 && bestmove.Valid() )
                bestmove_so_far = bestmove;
            int score_cp = (score*10)/BALANCE;  // convert to centipawns
            if( score_cp > 30000 )
                score_cp = 30000 + (score_cp-30000)/10000;
            else if( score < -30000 )
                score_cp = -30000 + (score_cp+30000)/10000;
            the_position.GetPV( pv );
            ReportOnProgress
            (
                false,
                multi,
                pv,
                score_cp,
                depth
            );
        }
    } 
    if( gbl_stop && bestmove_so_far.Valid() )
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
                               bestmove_so_far.TerseOut().c_str() ); 
    }
#endif
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
        file_log = fopen("/Users/Bill/Documents/Github/sargon/sargon-log-file.txt","wt" );
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
