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
#include <chrono>
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

    // Is queue empty ?
    bool empty()  { return q.empty(); }

    // Add an element to the queue.
    void enqueue(T t)
    {
        std::lock_guard<std::mutex> lock(m);
        q.push(t);
        c.notify_one();
    }

    // Get the "front" element.
    //  if the queue is empty, wait until an element is available
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

void timer_thread();
void read_stdin();
void write_stdout();

 
static SafeQueue<std::string> async_queue;
static std::mutex future_time_mtx;
static long future_time;
void set_future_time( long t )
{
    std::lock_guard<std::mutex> lck(future_time_mtx);
    future_time = t;
}

int main( int argc, char *argv[] )
{
    std::string filename_base( argv[0] );
    logfile_name = filename_base + "-log.txt";
    std::thread first (read_stdin);
    std::thread second (write_stdout);
    std::thread third (timer_thread);

    // Wait for main threads to finish
    first.join();                // pauses until first finishes
    second.join();               // pauses until second finishes

    // Tell timer thread to finish, then wait for it too
    set_future_time(-1);
    third.join();
    return 0;
}

void timer_thread()
{
    for(;;)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if( future_time == -1 )
            break;
        else if( future_time != 0 )
        {
            long now_time = GetTickCount();	
            long time_remaining = future_time - now_time;
            if( time_remaining < 0 )
            {
                set_future_time(0);
                async_queue.enqueue( "TIMEOUT" );
            }
        }
    }
}

void SetTimer( int ms )
{
    // Prevent generation of a stale "TIMEOUT" event
    set_future_time(0);

    // Remove stale "TIMEOUT" events from queue
    std::queue<std::string> temp;
    while( !async_queue.empty() )
    {
        std::string s = async_queue.dequeue();
        if( s != "TIMEOUT" )
            temp.push(s);
    }
    while( !temp.empty() )
    {
        std::string s = temp.front();
        temp.pop();
        async_queue.enqueue(s);
    }

    // Schedule a new "TIMEOUT" event
    if( ms != 0 )
    {
        long now_time = GetTickCount();	
        long ft = now_time + ms;
        if( ft==0 || ft==-1 )   // avoid special values
            ft = 1;             //  at cost of 1 or 2 millisecs of error!
        set_future_time(ft);
    }
}


void read_stdin()
{
#if 1
    static const char *test_sequence[] =
    {
        "uci\n",
        "setoption name MultiPV value 4\n",
        "isready\n",
        "position fen 7k/8/8/4R3/8/8/8/7K w - - 0 1\n",
        "go infinite\n"
    };
    for( int i=0; i<sizeof(test_sequence)/sizeof(test_sequence[0]); i++ )
    {
        std::string s(test_sequence[i]);
        async_queue.enqueue(s);
    }
#endif
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
        }
    }
}

void write_stdout()
{
    bool quit=false;
    while(!quit)
    {
        std::string s = async_queue.dequeue();
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
    if( str_pattern(buf,"TIMEOUT") )
        log( "TIMEOUT event\n" );
    else if( str_pattern(buf,"quit") )
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
        log( "rsp>%s\n", rsp );
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
        log( "rsp>%s\n", buf );
    }
}

/*

    Calculate next move efficiently using the available time.

    What is the time management algorithm?

    Each move we loop increasing plymax. We set a timer to cut us off if
    we spend too long.

    Basic idea is to loop the same number of times as on the previous
    move - i.e. we target the same plymax. The first time through
    we don't know the target unfortunately, so use the cut off timer
    to set the initial target. Later on the cut off timer should only
    kick in for unexpectedly long calculations (because we expect to
    take about the same amount of time as last time).
    
    Our algorithm is;

    loop
      if first time through
        set CUT to 1/20 of total time, establishes target
      else
        set CUT to 1/10 of total time, loop to target
        if hit target in less that 1/100 total time
          increment target and loop again
        if we are cut
          decrement target

    in this call 1/100, 1/20 and 1/10 thresholds LO, MED and HI

*/

thc::Move CalculateNextMove( bool new_game, unsigned long ms_time, unsigned long ms_inc )
{
    log( "Input ms_time=%d, ms_inc=%d\n", ms_time, ms_inc );
    static int plymax_target;
    const unsigned long LO =100;
    const unsigned long MED=20;
    const unsigned long HI =10;
    unsigned long ms_lo  = ms_time / LO;
    unsigned long ms_med = ms_time / MED;
    unsigned long ms_hi  = ms_time / HI;

    // Use the cut off timer, with a medium cutoff if we haven't yet
    //  established a target plymax
    if( new_game || plymax_target == 0 )
    {
        plymax_target = 0;
        SetTimer( ms_med );
    }

    // Else the cut off timer, is more of an emergency brake, and normally
    //  we just re-run Sargon until we hit plymax_target
    else
    {
        SetTimer( ms_hi );
    }
    int plymax = 3;
    std::string bestmove_terse;
    unsigned long base = GetTickCount();
    for(;;)
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
        bool aborted = RunSargon();
        unsigned long now = GetTickCount();
        unsigned long elapsed = (now-base);
        if( aborted  || the_pv.variation.size()==0 )
        {
            log( "aborted=%s, pv.variation.size()=%d, elapsed=%lu, ms_lo=%lu, plymax=%d, plymax_target=%d\n",
                    aborted?"true @@":"false", the_pv.variation.size(), //@@ marks move in log
                    elapsed, ms_lo, plymax, plymax_target );
        }

        // Report on each normally concluded iteration
        if( !aborted )
            ProgressReport();

        // Check for situations where Sargon minimax never ran
        if( the_pv.variation.size() == 0 )    
        {
            // maybe book move
            bestmove_terse = sargon_export_move(BESTM);
            plymax = 0; // to set plymax_target for next time
            break;
        }
        else
        {

            // If we have a move, and it checkmates opponent, play it!
            bestmove_terse = the_pv.variation[0].TerseOut();
            std::string bestm = sargon_export_move(BESTM);
            if( !aborted && bestmove_terse.substr(0,4) != bestm )
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

            // If we timed out, target plymax should be reduced
            if( aborted )
            {
                plymax--;
                break;
            }

            // Otherwise keep iterating or not, according to the time management algorithm
            bool keep_going = false;
            if( plymax_target<=0 || plymax<plymax_target )
                keep_going = true;  // no target or haven't reached target
            if( plymax_target>0 && plymax>=plymax_target && elapsed<ms_lo )
                keep_going = true;  // reached target very quickly, so extend target
            log( "elapsed=%lu, ms_lo=%lu, plymax=%d, plymax_target=%d, keep_going=%s\n", elapsed, ms_lo, plymax, plymax_target, keep_going?"true":"false @@" );  // @@ marks move in log
            if( !keep_going )
                break;
            plymax++;
        }
    }
    SetTimer(0);    // clear timer
    plymax_target = plymax;
    thc::Move bestmove;
    bestmove.Invalid();
    bool have_move = bestmove.TerseIn( &the_position, bestmove_terse.c_str() );
    if( !have_move )
        log( "Sargon doesn't find move - %s\n%s", bestmove_terse.c_str(), the_position.ToDebugStr().c_str() );
    return bestmove;
}

const char *cmd_go( const char *cmd )
{
    stop_rsp_buf[0] = '\0';
    static char buf[128];
    the_pv.clear();

    // Work out our time and increment
    // eg cmd ="wtime 30000 btime 30000 winc 0 binc 0"
    const char *stime;
    const char *sinc;
    int ms_time   = 0;
    int ms_inc    = 0;
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
    the_pv.clear();
    while( !aborted )
    {
        pokeb(MVEMSG,   0 );
        pokeb(MVEMSG+1, 0 );
        pokeb(MVEMSG+2, 0 );
        pokeb(MVEMSG+3, 0 );
        pokeb(PLYMAX, plymax++);
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

static bool cmd_position_is_new_game;
static bool is_new_game()
{
    return cmd_position_is_new_game;
}

const char *cmd_position( const char *cmd )
{
    static thc::ChessRules prev_position;
    bool position_changed = true;
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
            thc::ChessPosition initial;
            if( the_position == initial )
                position_changed = true;
            else if( the_position == prev_position )
                position_changed = false;

            // Maybe this latest position is the old one with one new move ?
            else if( last_move.Valid() )
            {
                thc::ChessRules temp = prev_position;
                temp.PlayMove( last_move );
                if( the_position == temp )
                {
                    // Yes it is! so we are still playing the same game
                    position_changed = false;
                }

                // Maybe this latest position is the old one with two new moves ?
                else if( last_move_but_one.Valid() )
                {
                    prev_position.PlayMove( last_move_but_one );
                    prev_position.PlayMove( last_move );
                    if( the_position == prev_position )
                    {
                        // Yes it is! so we are still playing the same game
                        position_changed = false;
                    }
                }
            }
        }
    }

    cmd_position_is_new_game = position_changed;
    log( "cmd_position(): %s\nSetting cmd_position_is_new_game=%s, %s",
        cmd,
        cmd_position_is_new_game?"true":"false",
        the_position.ToDebugStr().c_str() );

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

        // Abort RunSargon() if new event in queue (and it has found something)
        if( !async_queue.empty() && the_pv.variation.size()>0 )
        {
            longjmp( jmp_buf_env, 1 );
        }
    }
};

