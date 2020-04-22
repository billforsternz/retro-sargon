/*

  A primitive Windows UCI chess engine interface around the classic program Sargon,
  as presented in the book "Sargon a Z80 Computer Chess Program" by Dan and Kathe
  Spracklen (Hayden Books 1978). Another program in this suite converts the Z80 code
  to working X86 assembly language.
  
  */

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

// Measure elapsed time, nodes    
static unsigned long base_time;

// Misc
static int depth_option;    // 0=auto, other values for fixed depth play
static std::string logfile_name;
static std::vector< NODE > nodes;
static unsigned long total_callbacks;
static unsigned long bestmove_callbacks;
static unsigned long end_of_points_callbacks;

// The current 'Master' postion
static thc::ChessRules the_position;

// Command line interface
static bool process( const std::string &s );
static std::string cmd_uci();
static std::string cmd_isready();
static std::string cmd_stop();
static std::string cmd_go( const std::vector<std::string> &fields );
static void        cmd_go_infinite();
static void        cmd_setoption( const std::vector<std::string> &fields );
static void        cmd_position( const std::string &whole_cmd_line, const std::vector<std::string> &fields );

// Misc
static bool is_new_game();
static void log( const char *fmt, ... );
static bool RunSargon();
static void BuildPV( PV &pv );
static void ProgressReport();
static thc::Move CalculateNextMove( bool new_game, unsigned long ms_time, unsigned long ms_inc );

// A threadsafe-queue. (from https://stackoverflow.com/questions/15278343/c11-thread-safe-queue )
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

// Threading declarations
static SafeQueue<std::string> async_queue;
static void timer_thread();
static void read_stdin();
static void write_stdout();
static void TimerClear();          // Clear the timer
static void TimerEnd();            // End the timer subsystem system
static void TimerSet( int ms );    // Set a timeout event, ms millisecs into the future (0 and -1 are special values)


// main()
int main( int argc, char *argv[] )
{
    std::string filename_base( argv[0] );
    logfile_name = filename_base + "-log.txt";
#ifdef _DEBUG
    depth_option = 5;
#if 1 // preserve something that definitely fails, before embarking on mods
    static const char *test_sequence[] =
    {
        "uci\n",
        "isready\n",
        "position startpos moves c2c4 e7e5 g2g3 b8c6 b1c3 g8f6 g1f3 e5e4 f3h4 f8c5 f1g2 g7g5 d2d4 g5h4 d4c5 h4g3 h2g3 d8e7 c3d5 f6d5 c4d5 c6e5 d5d6 c7d6 c5d6 e7e6 g2e4 f7f5 h1h6 e5g6 e4d5 e6e5 d1d3 e8f8 h6h5 e5d6 h5f5 f8g7 f5f7\n",
        "go wtime 179297 btime 96328 winc 0 binc 0\n",
        "position startpos moves c2c4 e7e5 g2g3 b8c6 b1c3 g8f6 g1f3 e5e4 f3h4 f8c5 f1g2 g7g5 d2d4 g5h4 d4c5 h4g3 h2g3 d8e7 c3d5 f6d5 c4d5 c6e5 d5d6 c7d6 c5d6 e7e6 g2e4 f7f5 h1h6 e5g6 e4d5 e6e5 d1d3 e8f8 h6h5 e5d6 h5f5 f8g7 f5f7 g7g8 f7d7\n",
        "go wtime 179297 btime 84161 winc 0 binc 0\n",
        "position startpos moves c2c4 e7e5 g2g3 b8c6 b1c3 g8f6 g1f3 e5e4 f3h4 f8c5 f1g2 g7g5 d2d4 g5h4 d4c5 h4g3 h2g3 d8e7 c3d5 f6d5 c4d5 c6e5 d5d6 c7d6 c5d6 e7e6 g2e4 f7f5 h1h6 e5g6 e4d5 e6e5 d1d3 e8f8 h6h5 e5d6 h5f5 f8g7 f5f7 g7g8 f7d7 g8f8 d3f5\n",
        "go wtime 179177 btime 82213 winc 0 binc 0\n",
        "ucinewgame\n",
        "isready\n",
        "position startpos\n",
        "go wtime 300000 btime 300000 winc 0 binc 0\n"
    };
#endif
    for( int i=0; i<sizeof(test_sequence)/sizeof(test_sequence[0]); i++ )
    {
        std::string s(test_sequence[i]);
        util::rtrim(s);
        log( "cmd>%s\n", s.c_str() );
        process(s);
    }
    return 0;
#endif
    std::thread first(read_stdin);
    std::thread second(write_stdout);
    std::thread third(timer_thread);

    // Wait for main threads to finish
    first.join();                // pauses until first finishes
    second.join();               // pauses until second finishes

    // Tell timer thread to finish, then wait for it too
    TimerEnd();
    third.join();
    return 0;
}

static unsigned long base_time_sargon_execution, max_gap_so_far, max_len_so_far, max_variance_so_far;
static std::chrono::time_point<std::chrono::steady_clock> base = std::chrono::steady_clock::now();
static unsigned long bios_base = GetTickCount();
static unsigned long elapsed_milliseconds()
{
    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - base);
    unsigned long ret = static_cast<unsigned long>(ms.count());
    unsigned long bios_now = GetTickCount();
    unsigned long bios_ret = bios_now-bios_base;
    if( bios_ret > ret && bios_ret-ret > max_variance_so_far )
        max_variance_so_far = bios_ret-ret;
    else if( ret > bios_ret && ret-bios_ret > max_variance_so_far )
        max_variance_so_far = ret-bios_ret;
    return ret;
}

// Very simple timer thread, controlled by TimerSet(), TimerClear(), TimerEnd() 
static std::mutex timer_mtx;
static long future_time;
static void timer_thread()
{
    for(;;)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        {
            std::lock_guard<std::mutex> lck(timer_mtx);
            if( future_time == -1 )
                break;
            else if( future_time != 0 )
            {
                long now_time = elapsed_milliseconds();	
                long time_remaining = future_time - now_time;
                if( time_remaining <= 0 )
                {
                    future_time = 0;
                    async_queue.enqueue( "TIMEOUT" );
                }
            }
        }
    }
}

// Clear the timer
static void TimerClear()
{
    TimerSet(0);  // set sentinel value 0
}

// End the timer subsystem system
static void TimerEnd()
{
    TimerSet(-1);  // set sentinel value -1
}

// Set a timeout event, ms millisecs into the future (0 and -1 are special values)
static void TimerSet( int ms )
{
    std::lock_guard<std::mutex> lck(timer_mtx);

    // Prevent generation of a stale "TIMEOUT" event
    future_time = 0;

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

    // Check for TimerEnd()
    if( ms == -1 )
        future_time = -1;

    // Schedule a new "TIMEOUT" event (unless 0 = TimerClear())
    else if( ms != 0 )
    {
        long now_time = elapsed_milliseconds();	
        long ft = now_time + ms;
        if( ft==0 || ft==-1 )   // avoid special values
            ft = 1;             //  at cost of 1 or 2 millisecs of error!
        future_time = ft;
    }
}

// Read commands from stdin and queue them
static void read_stdin()
{
    bool quit=false;
#if 0
    static const char *test_sequence[] =
    {
        "uci\n",
        "isready\n",
        "position fen rn2k1nr/1p4pp/4bp2/1P2p3/p2pN3/b1qP1NP1/4PPBP/1RBQ1RK1 b kq - 1 13\n",
        "go infinite\n",
        "stop\n",
        //"quit\n"
    };
#endif
    while(!quit)
    {
        static char buf[8192];
        if( NULL == fgets(buf,sizeof(buf)-2,stdin) )
            quit = true;
        else
        {
            std::string s(buf);
            util::rtrim(s);
            async_queue.enqueue(s);
            if( s == "quit" )
                quit = true;
        }
    }
}

// Read queued commands and process them
static void write_stdout()
{
    bool quit=false;
    while(!quit)
    {
        std::string s = async_queue.dequeue();
        log( "cmd>%s\n", s.c_str() );
        quit = process(s);
    }
} 

// Run Sargon analysis, until completion or timer abort (see callback() for timer abort)
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
        base_time_sargon_execution = elapsed_milliseconds();
        sargon(api_CPTRMV);
        the_pv = provisional;
    }
    return aborted;
}

// Command line top level handler
static bool process( const std::string &s )
{
    bool quit=false;
    std::string rsp;
    std::vector<std::string> fields_raw, fields;
    util::split( s, fields_raw );
    for( std::string f: fields_raw )
        fields.push_back( util::tolower(f) );
    if( fields.size() == 0 )
        return false;
    std::string cmd = fields[0];
    std::string parm1 = fields.size()<=1 ? "" : fields[1];
    if( cmd == "timeout" )
        log( "TIMEOUT event\n" );
    else if( cmd == "quit" )
        quit = true;
    else if( cmd == "uci" )
        rsp = cmd_uci();
    else if( cmd == "isready" )
        rsp = cmd_isready();
    else if( cmd == "stop" )
        rsp = cmd_stop();
    else if( cmd=="go" && parm1=="infinite" )
        cmd_go_infinite();
    else if( cmd=="go" )
        rsp = cmd_go(fields);
    else if( cmd=="setoption" )
        cmd_setoption(fields);
    else if( cmd=="position" )
        cmd_position( s, fields );
    if( rsp != "" )
    {
        log( "rsp>%s\n", rsp.c_str() );
        fprintf( stdout, "%s", rsp.c_str() );
        fflush( stdout );
    }
    log( "function process() returns, cmd=%s\n"
         "total callbacks=%lu\n"
         "bestmove callbacks=%lu\n"
         "end of points callbacks=%lu\n"
         "max_variance_so_far=%lu\n"
         "max length of PV vector=%lu\n"
         "max time between bestmove callbacks=%lu\n",
            cmd.c_str(),
            total_callbacks,
            bestmove_callbacks,
            end_of_points_callbacks,
            max_variance_so_far,
            max_len_so_far,
            max_gap_so_far );
    return quit;
}

static std::string cmd_uci()
{
    std::string rsp=
    "id name " ENGINE_NAME " " VERSION "\n"
    "id author Dan and Kathe Spracklin, Windows port by Bill Forster\n"
    "option name Depth type spin min 0 max 20 default 0\n"
    "uciok\n";
    return rsp;
}

static std::string cmd_isready()
{
    return "readyok\n";
}

static std::string stop_rsp;
static std::string cmd_stop()
{
    return stop_rsp;
}

static void cmd_setoption( const std::vector<std::string> &fields )
{
    // Support single option "Depth"
    //  Depth is 0-20, default is 0. 0 indicates auto depth selection,
    //   others are fixed depth

    // eg "setoption name Depth value 3"
    if( fields.size()>4 && fields[1]=="name" && fields[2]=="depth" && fields[3]=="value" )
    {
        depth_option = atoi(fields[4].c_str());
        if( depth_option<0 || depth_option>20 )
            depth_option = 0;
    }
}

static std::string cmd_go( const std::vector<std::string> &fields )
{
    stop_rsp = "";
    the_pv.clear();
    provisional.clear();
    base_time = elapsed_milliseconds();
    total_callbacks = 0;
    bestmove_callbacks = 0;
    end_of_points_callbacks = 0;

    // Work out our time and increment
    // eg cmd ="wtime 30000 btime 30000 winc 0 binc 0"
    std::string stime = "btime";
    std::string sinc  = "binc";
    if( the_position.white )
    {
        stime = "wtime";
        sinc  = "winc";
    }
    bool expecting_time = false;
    bool expecting_inc = false;
    int ms_time   = 0;
    int ms_inc    = 0;
    for( std::string parm: fields )
    {
        if( expecting_time )
        {
            ms_time = atoi(parm.c_str());
            expecting_time = 0;
        }
        else if( expecting_inc )
        {
            ms_inc = atoi(parm.c_str());
            expecting_inc = 0;
        }
        else
        {
            if( parm == stime )
                expecting_time = true;
            if( parm == sinc )
                expecting_inc = true;
        }
    }
    bool new_game = is_new_game();
    thc::Move bestmove = CalculateNextMove( new_game, ms_time, ms_inc );
    return util::sprintf( "bestmove %s\n", bestmove.TerseOut().c_str() );
}

static void cmd_go_infinite()
{
    stop_rsp = "";
    int plymax=3;
    bool aborted = false;
    the_pv.clear();
    provisional.clear();
    base_time = elapsed_milliseconds();
    total_callbacks = 0;
    bestmove_callbacks = 0;
    end_of_points_callbacks = 0;
    while( !aborted && plymax<=20 )
    {
        pokeb(PLYMAX, plymax++);
        pokeb(MLPTRJ,0);
        pokeb(MLPTRJ+1,0);
        sargon(api_INITBD);
        sargon_import_position(the_position,true);  // note avoid_book = true
        log( "Sargon's internal MOVENO set to %d\n", peekb(MOVENO) );
        sargon(api_ROYALT);
        pokeb( KOLOR, the_position.white ? 0 : 0x80 );    // Sargon is side to move
        nodes.clear();
        aborted = RunSargon();
        if( !aborted )
            ProgressReport();   
    }
}

// cmd_position(), set a new (or same or same plus one or two half moves) position
static bool cmd_position_signals_new_game;
static bool is_new_game()
{
    return cmd_position_signals_new_game;
}

static void cmd_position( const std::string &whole_cmd_line, const std::vector<std::string> &fields )
{
    static thc::ChessRules prev_position;
    bool position_changed = true;

    // Get base starting position
    thc::ChessRules tmp;
    the_position = tmp;    //init
    bool look_for_moves = false;
    if( fields.size() > 2 && fields[1]=="fen" )
    {
        size_t offset = whole_cmd_line.find("fen");
        offset = whole_cmd_line.find_first_not_of(" \t",offset+3);
        if( offset != std::string::npos )
        {
            std::string fen = whole_cmd_line.substr(offset);
            the_position.Forsyth(fen.c_str());
            look_for_moves = true;
        }
    }
    else if( fields.size() > 1 && fields[1]=="startpos" )
    {
        thc::ChessRules tmp;
        the_position = tmp;    //init
        look_for_moves = true;
    }

    // Add moves
    if( look_for_moves )
    {
        bool expect_move = false;
        thc::Move last_move, last_move_but_one;
        last_move_but_one.Invalid();
        last_move.Invalid();
        for( std::string parm: fields )
        {
            if( expect_move )
            {
                thc::Move move;
                bool okay = move.TerseIn(&the_position,parm.c_str());
                if( !okay )
                    break;
                the_position.PlayMove( move );
                last_move_but_one = last_move;
                last_move         = move;
            }
            else if( parm == "moves" )
                expect_move = true;
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

    cmd_position_signals_new_game = position_changed;
    log( "cmd_position(): %s\nSetting cmd_position_signals_new_game=%s\nFEN = %s\n%s",
        whole_cmd_line.c_str(),
        cmd_position_signals_new_game?"true":"false",
        the_position.ForsythPublish().c_str(),
        the_position.ToDebugStr().c_str() );

    // For next time
    prev_position = the_position;
}

static void ProgressReport()
{
    int     score_cp   = the_pv.value;
    int     depth      = the_pv.depth;
    thc::ChessRules ce = the_position;
    int score_overide;
    std::string buf_pv;
    std::string buf_score;
    bool done=false;
    unsigned long now_time = elapsed_milliseconds();	
    int nodes = end_of_points_callbacks;
    unsigned long elapsed_time = now_time-base_time;
    if( elapsed_time == 0 )
        elapsed_time++;
    bool overide = false;
    for( unsigned int i=0; i<the_pv.variation.size(); i++ )
    {
        bool okay;
        thc::Move move=the_pv.variation[i];
        ce.PlayMove( move );
        buf_pv += " ";
        buf_pv += move.TerseOut();
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
                buf_score = util::sprintf( "mate %d", score_overide );
            else if( score_overide < 0 ) // are me being mated ?
                buf_score = util::sprintf( "mate -%d", (0-score_overide) );
            else if( score_overide == 0 ) // is it a stalemate draw ?
                buf_score = util::sprintf( "cp 0" );
        }
        else
        {
            buf_score = util::sprintf( "cp %d", score_cp );
        }
    }
    else
    {
        if( overide ) 
        {
            if( score_overide < 0 ) // are we mating ?
                buf_score = util::sprintf( "mate %d", 0-score_overide );
            else if( score_overide > 0 ) // are me being mated ?        
                buf_score = util::sprintf( "mate -%d", score_overide );
            else if( score_overide == 0 ) // is it a stalemate draw ?
                buf_score = util::sprintf( "cp 0" );
        }
        else
        {
            buf_score = util::sprintf( "cp %d", 0-score_cp );
        }
    }
    if( the_pv.variation.size() > 0 )
    {
        std::string out = util::sprintf( "info depth %d score %s hashfull 0 time %lu nodes %lu nps %lu pv%s\n",
                    depth,
                    buf_score.c_str(),
                    (unsigned long) elapsed_time,
                    (unsigned long) nodes,
                    1000L * ((unsigned long) nodes / (unsigned long)elapsed_time ),
                    buf_pv.c_str() );
        fprintf( stdout, out.c_str() );
        fflush( stdout );
        log( "rsp>%s\n", out.c_str() );
        stop_rsp = util::sprintf( "bestmove %s\n", the_pv.variation[0].TerseOut().c_str() ); 
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
        set CUT to 1/15 of total time, establishes target
      else
        set CUT to 1/8 of total time, loop to target
        if hit target in less that 1/50 total time
          increment target and loop again
        if we are cut
          decrement target

    in this call 1/50, 1/15 and 1/8 thresholds LO, MED and HI

*/

static thc::Move CalculateNextMove( bool new_game, unsigned long ms_time, unsigned long ms_inc )
{
    log( "Input ms_time=%d, ms_inc=%d\n", ms_time, ms_inc );
    static int plymax_target;
    const unsigned long LO =50;
    const unsigned long MED=15;
    const unsigned long HI =8;
    unsigned long ms_lo  = ms_time / LO;
    unsigned long ms_med = ms_time / MED;
    unsigned long ms_hi  = ms_time / HI;

    // Use the cut off timer, with a medium cutoff if we haven't yet
    //  established a target plymax
    if( new_game || plymax_target == 0 )
    {
        plymax_target = 0;
        TimerSet( ms_med );
    }

    // Else the cut off timer is more of an emergency brake, and normally
    //  we just re-run Sargon until we hit plymax_target
    else
    {
        TimerSet( ms_hi );
    }
    bool fixed_depth = (depth_option>0);
    int plymax = fixed_depth ? depth_option : 3;
    int stalemates = 0;
    std::string bestmove_terse;
    unsigned long base = elapsed_milliseconds();
    for(;;)
    {
        pokeb(MVEMSG,   0 );
        pokeb(MVEMSG+1, 0 );
        pokeb(MVEMSG+2, 0 );
        pokeb(MVEMSG+3, 0 );
        pokeb(MLPTRJ,0);
        pokeb(MLPTRJ+1,0);
        pokeb(PLYMAX, plymax );
        sargon(api_INITBD);
        sargon_import_position(the_position);
        sargon(api_ROYALT);
        pokeb( KOLOR, the_position.white ? 0 : 0x80 );    // Sargon is side to move
        nodes.clear();
        bool aborted = RunSargon();
        unsigned long now = elapsed_milliseconds();
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
                if( score_terminal == thc::TERMINAL_BSTALEMATE ||
                    score_terminal == thc::TERMINAL_WSTALEMATE )
                {
                    stalemates++;
                }
            }

            // If fixed depth, never iterate
            if( fixed_depth )
                break;

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
            else if( plymax_target>0 && plymax>=plymax_target && elapsed<ms_lo )
                keep_going = true;  // reached target very quickly, so extend target
            else if( stalemates == 1 )
                keep_going = true;  // try one more ply if we stalemate opponent!
            log( "elapsed=%lu, ms_lo=%lu, plymax=%d, plymax_target=%d, keep_going=%s\n", elapsed, ms_lo, plymax, plymax_target, keep_going?"true":"false @@" );  // @@ marks move in log
            if( !keep_going )
                break;
            plymax++;
        }
    }
    TimerClear();
    plymax_target = plymax;
    thc::Move bestmove;
    bestmove.Invalid();
    bool have_move = bestmove.TerseIn( &the_position, bestmove_terse.c_str() );
    if( !have_move )
        log( "Sargon doesn't find move - %s\n%s", bestmove_terse.c_str(), the_position.ToDebugStr().c_str() );
    return bestmove;
}

// Simple logging facility gives us some debug capability when running under control of a GUI
static void log( const char *fmt, ... )
{
    static std::mutex mtx;
    std::lock_guard<std::mutex> lck(mtx);
	va_list args;
	va_start( args, fmt );
    static bool first=true;
    FILE *file_log;
    errno_t err = fopen_s( &file_log, logfile_name.c_str(), first? "wt" : "at" );
    first = false;
    if( !err )
    {
        static char buf[1024];
        time_t t = time(NULL);
        struct tm ptm;
        localtime_s( &ptm, &t );
        asctime_s( buf, sizeof(buf), &ptm );
        char *p = strchr(buf,'\n');
        if( p )
            *p = '\0';
        fputs( buf, file_log);
        buf[0] = ':';
        buf[1] = ' ';
        vsnprintf( buf+2, sizeof(buf)-4, fmt, args ); 
        fputs( buf, file_log );
        fclose( file_log );
    }
    va_end(args);
}

// Use our knowledge for the way Sargon does minimax/alpha-beta to build a PV
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

            //if( target == plymax ) // commented out to allow extra depth nodes in case of checks - see below *
            //    break;
            target++;
        }
    }
    thc::ChessRules cr = the_position;
    nbr = nodes_pv.size();
    bool ok = true;
    for( int i=0; ok && i<nbr; i++ )
    {
        // See above *
        // Normally we expect plymax nodes. So plymax=3; 3 nodes at level 1,2,3; i=0,1,2.
        // But Sargon does an extra ply if the king is in check, so allow (but don't insist on)
        // extra node(s) if terminal node is check.
        // Example Fen: r1bqk2r/pppp1ppp/2n5/2b4n/4Pp2/2NP1N2/PPPBB1PP/R2QK2R b KQkq - 6 7
        // PV at plymax=5 is: Sargon 1978 -0.97 (depth 5) 7...Qf6 8.Ng1 Bxg1 9.Rxg1 Qh4+ 10.Kf1
        // Before the next paragraph of code was added 10.Kf1 didn't show in the line and the
        // score was -12.80 pawns, the leaf score of Qh4+, which presumably is allowed to be
        // exaggerated because of the check and alpha-beta optimisation (make sure checks are
        // evaluated first).
        if( i >= plymax )
        {
            thc::Square sq = (cr.WhiteToPlay() ? cr.wking_square : cr.bking_square);
            bool in_check = cr.AttackedPiece(sq);
            if( !in_check )
                break;
        }
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
                bool legal = mv.TerseIn( &cr, buf );
                if( !legal )
                {
                    log( "Unexpected illegal move=%s, Position=%s\n", buf, cr.ToDebugStr().c_str() );
                    log( "Extra info: Starting position leading to unexpected illegal move FEN=%s, position=\n%s\n", the_position.ForsythPublish().c_str(), the_position.ToDebugStr().c_str() );
                    std::string the_moves = "Moves leading to unexpected illegal move";
                    for( int j=0; j<=i; j++ )
                    {
                        NODE *q = &nodes_pv[j];
                        thc::Square qsrc, qdst;
                        sargon_export_square( q->from, qsrc );
                        sargon_export_square( q->to, qdst );
                        char buff[5];
                        buff[0] = thc::get_file(qsrc);
                        buff[1] = thc::get_rank(qsrc);
                        buff[2] = thc::get_file(qdst);
                        buff[3] = thc::get_rank(qdst);
                        buff[4] = '\0';
                        the_moves += " ";
                        the_moves += std::string(buff);
                    }
                    log( "Extra info: %s\n", the_moves.c_str() );
                    break;
                }
                else
                {
                    cr.PlayMove(mv);
                    pv.variation.push_back(mv);
                }
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

    // Values are calculated as a weighted combination of net material plus net mobility
    //  plus an adjustment for possible exchanges in the terminal position. The value is
    //  also *relative* to the ply0 score
    //  We want to present the *absolute* value in centipawns, so we need to know the
    //  ply0 score. It is the same weighted combination of net material plus net mobility.
    //  The actual ply0 score is available, but since it also adds the possible exchanges
    //  adjustment and we don't want that, calculate the weighted combination of net
    //  material and net mobility at ply0 instead.
    char mv0 = peekb(MV0);      // net ply 0 material (pawn=2, knight/bishop=6, rook=10...)
    if( mv0 > 30 )              // Sargon limits this to +-30 (so 15 pawns) to avoid overflow
        mv0 = 30;
    if( mv0 < -30 )
        mv0 = -30;
    char bc0 = peekb(BC0);      // net ply 0 mobility
    if( bc0 > 6 )               // also limited to +-6
        bc0 = 6;
    if( bc0 < -6 )
        bc0 = -6;
    int ply0 = mv0*4 + bc0;     // Material gets 4 times weight as mobility (4*30 + 6 = 126 doesn't overflow signed char)
    double centipawns_ply0 = ply0 * 100.0/8.0;   // pawn is 2*4 = 8 -> 100 centipawns 

    // Avoid this apparently simpler alternative, because don't want exchange adjustment at ply 0
#if 0
    double fvalue_ply0 = sargon_export_value( peekb(5) ); //Where root node value ends up if MLPTRJ=0, which it does initially
#endif

    // So actual value is ply0 + score relative to ply0
    pv.value = static_cast<int>(centipawns_ply0+centipawns);
    log( "centipawns=%f, centipawns_ply0=%f, plymax=%d\n", centipawns, centipawns_ply0, plymax );
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
        total_callbacks++;
#if 0
        thc::ChessPosition cp;
        unsigned char al = reg_eax & 0xff;
        if( 0 == strcmp(msg,"MATERIAL") )
        {
            sargon_export_position(cp);
            log( "Position is %s\n", cp.ToDebugStr().c_str() );
            log( "MATERIAL=0x%02x\n", al );
        }
        else if( 0 == strcmp(msg,"SUM") )
        {
            char val = reg_eax & 0xff;
            sargon_export_position(cp);
            log( "Adding material in loop, piece=%d\n", val );
        }
        else if( 0 == strcmp(msg,"MATERIAL - PLY0") )
        {
            log( "MATERIAL-PLY0=0x%02x\n", al );
        }
        else if( 0 == strcmp(msg,"MATERIAL LIMITED") )
        {
            log( "MATERIAL LIMITED=0x%02x\n", al );
        }
        else if( 0 == strcmp(msg,"MOBILITY") )
        {
            log( "MOBILITY=0x%02x\n", al );
        }
        else if( 0 == strcmp(msg,"MOBILITY - PLY0") )
        {
            log( "MOBILITY - PLY0=0x%02x\n", al );
        }
        else if( 0 == strcmp(msg,"MOBILITY LIMITED") )
        {
            log( "MOBILITY LIMITED=0x%02x\n", al );
        }
        else if( 0 == strcmp(msg,"end of POINTS()") )
        {
            log( "val=0x%02x\n", al );
        }
        else
#endif
        if( 0 == strcmp(msg,"end of POINTS()") )
        {
            end_of_points_callbacks++;
            unsigned long now = elapsed_milliseconds();
            unsigned long elapsed = now - base_time_sargon_execution;
            base_time_sargon_execution = now;
            if( elapsed > max_gap_so_far )
                max_gap_so_far = elapsed;
        }
        else if( 0 == strcmp(msg,"Yes! Best move") )
        {
            bestmove_callbacks++;
         /* unsigned long now = elapsed_milliseconds();
            unsigned long elapsed = now - base_time_sargon_execution;
            base_time_sargon_execution = now;
            if( elapsed > max_gap_so_far )
                max_gap_so_far = elapsed; */
            unsigned int p      = peekw(MLPTRJ);
            unsigned int level  = peekb(NPLY);
            unsigned char from  = peekb(p+2);
            unsigned char to    = peekb(p+3);
            unsigned char flags = peekb(p+4);
            unsigned char value = peekb(p+5);
            NODE n(level,from,to,flags,value);
            nodes.push_back(n);
            if( nodes.size() > max_len_so_far )
                max_len_so_far = nodes.size();
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

