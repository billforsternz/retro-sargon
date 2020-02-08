/* NOTES:

   Consider a model where we start with depth 5 say,
   each move we check what % of our time we used. If more than
   10% reduce depth, if less that 2% increase depth. Also need
   effective dead mans brake.

*/

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
#include <queue>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "util.h"
#include "thc.h"
#include "sargon-interface.h"
#include "sargon-asm-interface.h"

//-- preferences
#define VERSION "1978"
#define ENGINE_NAME "Sargon"

// A move in Sargon's evaluation graph, in this program a move that is marked as
//  the best move found so far at a given level
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

// A principal variation
struct PV
{
    std::vector<thc::Move> variation;
    int value;
    int depth;
    void clear() {variation.clear(),value=0,depth=0;}
    PV () {clear();}
};

// When a node is indicated as 'BEST' at level one, we can look back through
//  previously indicated nodes at higher level and construct a PV
static PV the_pv;
static PV provisional;

static std::string logfile_name;
static std::vector< NODE > nodes;

// The current 'Master' postion
static thc::ChessRules the_position;

// Command line interface
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
static void BuildPV( PV &pv );

// A threadsafe-queue.
template <class T>
class SafeQueue
{
public:
    SafeQueue() : q(), m(), c() {}

    ~SafeQueue() {}

    // Add an element to the queue.
    void enqueue(T t)
    {
        std::lock_guard<std::mutex> lock(m);
        q.push(t);
        c.notify_one();
    }

    // Get the "front"-element.
    //  if the queue is empty, wait till a element is available
    T dequeue(void)
    {
        std::unique_lock<std::mutex> lock(m);
        while(q.empty())
        {
            // release lock as long as the wait and reaquire it afterwards
            c.wait(lock);
        }
        T val = q.front();
        q.pop();
        return val;
    }

private:
    std::queue<T> q;
    mutable std::mutex m;
    std::condition_variable c;
};


// A couple of threads and primitive shared memory ring buffer
#define BIGBUF 8192
#if 0
#define CMD_BUF_NBR 8
static HANDLE hSem;
static char cmdline[CMD_BUF_NBR][BIGBUF+2];
static int cmd_put;
static int cmd_get;
#endif
static bool signal_stop;

static const char *test_sequence[] =
{
    "uci\n",
    "setoption name MultiPV value 4\n",
    "isready\n",
    "position fen 8/8/q2pk3/2p5/8/3N4/8/4K2R w K - 0 1\n",
    "go infinite\n"
};

std::mutex m;
std::condition_variable cv;
std::string data;
bool ready = false;
bool processed = false;

void read_stdin();
void write_stdout();

 
int main()
{
    std::thread first (read_stdin);
    std::thread second (write_stdout);

    // Wait for both threads to finish
    first.join();                // pauses until first finishes
    second.join();               // pauses until second finishes
    return 0;
}

static SafeQueue<std::string> async_queue;

void read_stdin()
{
    bool quit=false;
    static char buf[BIGBUF+2];
    while(!quit)
    {
        if( NULL == fgets(buf,BIGBUF,stdin) )
            quit = true;
        else
        {
            std::string s(buf);
            util::rtrim(s);
            async_queue.enqueue(s);
            if( s=="quit" )
                quit = true;
            if( s=="stop" || s=="quit" )
                signal_stop = true;
        }
    }
}

void write_stdout()
{
    bool quit=false;
    while(!quit)
    {
        std::string s = async_queue.dequeue();
        signal_stop = false;
        const char *cmd = s.c_str();
        log( "cmd>%s\n", cmd );
        quit = process(cmd);
    }
} 

#if 0
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
#endif

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
        the_pv.clear();
        provisional.clear();
        sargon(api_CPTRMV);
        the_pv = provisional;
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

// Command line top level handler
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
    int     score_cp   = the_pv.value;
    int     depth      = the_pv.depth;
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
    for( unsigned int i=0; i<the_pv.variation.size(); i++ )
    {
        bool okay;
        thc::Move move=the_pv.variation[i];
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

/*

Concept:

Try to run with consistent n (=plymax) each move. For each move, establish a window,
repeat algorthithm with increasing n until elapsed time is in the window.

If we hit the window, but n is less than previous move, increase the window to give
us a chance of continuing to use the higher n.

If we hit the window, and n is greater than previous move, decrease the window

Window is LO to HI. LO is always 0.5 Goal. HI ranges from 1*Goal to 4* goal (2 initially)

goal   =  time/100 + inc
window =  lo to hi = LO*goal to HI*goal, LO = 0.5, HI = 2.0 initially
cutoff =  time/2 + inc or hi whichever is greater

RUN with n=plymax = 3 initially, increment for each repeat
    if t >= cutoff
        USE result
    if t < lo
        n++ and REPEAT
    else if t < hi
        if n == n_previous
            USE
        if n > n_previous
            reduce HI and USE
        if n < n_previous
            increase HI and REPEAT
    else if t >= hi
            increase HI and USE

*/

thc::Move CalculateNextMove( bool new_game, unsigned long ms_time, unsigned long ms_inc )
{
    thc::Move bestmove;
    log( "Input ms_time=%d, ms_inc=%d\n", ms_time, ms_inc );
    double lo_factor = 0.3;
    static double hi_factor;
    static int plymax_previous;
    static unsigned long divisor;
    if( new_game )
    {
        hi_factor = 3.0;
        plymax_previous = 3;
        divisor = 50;
    }
    unsigned long ms_goal=0, ms_cutoff=0, ms_lo=0, ms_hi=0, ms_mid=0;

    ms_goal = ms_time / divisor;
    /*

    if( ms_time == 0 )
        ms_goal = 0;
    else if( ms_inc == 0 )
        ms_goal = ms_time/40;
    else if( ms_time > 100*ms_inc )
        ms_goal = ms_time/30;
    else if( ms_time > 50*ms_inc )
        ms_goal = ms_time/20;
    else if( ms_time > 10*ms_inc )
        ms_goal = ms_time/10;
    else if( ms_time > ms_inc )
        ms_goal = ms_time/5;
    else
        ms_goal = ms_time/2; */

    // When time gets really short get serious
    if( ms_goal < 50 )
        ms_goal = 1;

    ms_cutoff = ms_time/2;
    if( ms_cutoff < ms_goal )
        ms_cutoff = ms_goal;
    ms_lo = static_cast<unsigned long>( static_cast<double>(ms_goal) * lo_factor );
    ms_hi = static_cast<unsigned long>( static_cast<double>(ms_goal) * hi_factor );
    ms_mid = (ms_lo+ms_hi) / 2;
    bool aborted = false;
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
        pokeb(PLYMAX, plymax );
        sargon(api_INITBD);
        sargon_import_position(the_position);
        int moveno=the_position.full_move_count;
        sargon(api_ROYALT);
        pokeb( KOLOR, the_position.white ? 0 : 0x80 );    // Sargon is side to move
        nodes.clear();
        aborted = RunSargon();
        log( "aborted=%s, pv.variation.size()=%d moveno=%d\n", aborted?"true":"false", the_pv.variation.size(), moveno );
        if( !aborted )
            ProgressReport();   
        if( the_pv.variation.size() == 0 )    
        {
            // maybe book move
            bestmove_terse = sargon_export_move(BESTM);
            break;
        }
        else
        {
            bestmove_terse = the_pv.variation[0].TerseOut();
            std::string bestm = sargon_export_move(BESTM);
            if( bestmove_terse != bestm )
            {
                log( "Unexpected event: BESTM=%s != PV[0]=%s\n%s", bestm.c_str(), bestmove_terse.c_str(), the_position.ToDebugStr().c_str() );
            }
            thc::TERMINAL score_terminal;
            thc::ChessRules ce = the_position;
            ce.PlayMove(the_pv.variation[0]);
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
            log( "elapsed=%lu, window=%lu-%lu, cutoff=%lu, plymax=%d\n", elapsed, ms_lo, ms_hi, ms_cutoff, plymax );

            // if t >= cutoff
            //   USE result
            if( elapsed > ms_cutoff )
            {
                log( "@USE@ elapsed > ms_cutoff, plymax = %d\n", plymax );
                break;
            }

            // else if t < lo
            //   n++ and REPEAT
            else if( elapsed < ms_lo )
            {
                plymax++;
                continue;
            }

            // else if t < hi
            else if( elapsed < ms_hi )
            {
                // if n >= n_previous
                //   USE
                if( plymax >= plymax_previous )
                {
                    bool upper_half = elapsed > ms_mid;
                    if( upper_half )
                    {
                        //divisor -= 2;
                        //if( divisor < 30 )
                            divisor = 30;
                    }
                    else
                    {
                        //divisor += 2;
                        //if( divisor > 100 )
                            divisor = 100;
                    }
                    log( "in window, @USE@: %s, plymax=%d, divisor=%lu\n", upper_half?"Upper half of range so reduce divisor"
                                                                                   :"Lower half of range so increase divisor"
                                    , plymax, divisor );
                    break;
                }

                // if n < n_previous
                //   REPEAT
                else if( plymax < plymax_previous )
                {
                    plymax++;
                    log( "in window CONTINUE: new plymax=%d, divisor=%lu\n", plymax, divisor );
                    continue;
                }
            }

            // else if t >= hi
            //   increase HI and USE
            else if( elapsed >= ms_hi )
            {
                divisor = 50;
                log( "beyond window @USE@: new plymax target=%d, divisor=%lu\n", --plymax, divisor );
                break;
            }
        }
    }
    plymax_previous = plymax;
    bool have_move = bestmove.TerseIn( &the_position, bestmove_terse.c_str() );
    if( !have_move )
    {
        log( "Sargon doesn't find move - %s\n%s", bestmove_terse.c_str(), the_position.ToDebugStr().c_str() );
        bestmove.Invalid();
    }
    return bestmove;
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

    bool new_game = is_new_game();
    thc::Move bestmove = CalculateNextMove( new_game, ms_time, ms_inc );
    ProgressReport();
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
        if( !aborted )
            ProgressReport();   
    }
    if( aborted && the_pv.variation.size() > 0 )
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
                               the_pv.variation[0].TerseOut().c_str() ); 
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
    static thc::ChessRules prev_position;
    different_game = true;
    log( "cmd_position(): %s\n", cmd );
    const char *s, *parm;

    // Get base starting position
    thc::ChessEngine tmp;
    the_position = tmp;    //init
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

    // Add moves
    if( look_for_moves )
    {
        s = strstr(parm,"moves ");
        if( s )
        {
            thc::Move last_move, last_move_but_one;
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
            log( "cmd_position(): position set = %s\n", the_position.ToDebugStr().c_str() );

            // Maybe this latest position is the old one with two new moves ?
            if( last_move_but_one.Valid() && last_move.Valid() )
            {
                prev_position.PlayMove( last_move_but_one );
                prev_position.PlayMove( last_move );
                if( the_position == prev_position )
                {
                    // Yes it is! so we are still playing the same game
                    different_game = false;
                    log( "cmd_position(): Setting different_game = false %s\n", the_position.ToDebugStr().c_str() );
                }
            }
        }
    }

    // For next time
    prev_position = the_position;
    return NULL;
}

static void log( const char *fmt, ... )
{
	va_list args;
	va_start( args, fmt );
    static bool first=true;
    FILE *file_log = fopen( logfile_name.c_str(), first? "wt" : "at" );
    first = false;
    if( file_log )
    {
        static char buf[1024];
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
    }
    va_end(args);
}

static void BuildPV( PV &pv )
{
    pv.variation.clear();
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
                pv.variation.push_back(mv);
            }
        }
    }
    pv.depth = plymax;
    double fvalue = sargon_export_value( nodes_pv[nbr-1].value );

    // Sargon's values are negated at alternate levels, transforming minimax to maximax.
    //  If White to move, maximise conventional values at level 0,2,4
    //  If Black to move, maximise negated values at level 0,2,4
    bool odd = ((nbr-1)%2 == 1);
    bool negate = the_position.WhiteToPlay() ? odd : !odd;
    double centipawns = (negate ? -100.0 : 100.0) * fvalue;
    pv.value = static_cast<int>(centipawns);
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
                BuildPV( provisional );
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

