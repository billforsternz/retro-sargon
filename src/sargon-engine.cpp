/****************************************************************************
 * This project is a Windows port of the classic program Sargon, as
 * presented in the book "Sargon a Z80 Computer Chess Program" by Dan
 * and Kathe Spracklen (Hayden Books 1978).
 *
 * File: sargon-engine.cpp
 *       A simple Windows UCI chess engine interface
 *
 * Bill Forster, https://github.com/billforsternz/retro-sargon
 ****************************************************************************/

#include <stdio.h>
#include <stdarg.h>
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
#include "sargon-pv.h"

// Measure elapsed time, nodes    
static unsigned long base_time;

// Misc
#define VERSION "1978 V1.01"
#define ENGINE_NAME "Sargon"
static int depth_option;    // 0=auto, other values for fixed depth play
static std::string logfile_name;
static unsigned long total_callbacks;
static unsigned long genmov_callbacks;
static unsigned long bestmove_callbacks;
static unsigned long end_of_points_callbacks;

// The current 'Master' postion
static thc::ChessRules the_position;

// The current 'Master' PV
static PV the_pv;

// Play down mating sequence without recalculation if possible
struct MATING
{
    bool                   active;
    thc::ChessRules        position;    // start position
    std::vector<thc::Move> variation;   // leads to mate
    unsigned int           idx;         // idx into variation
    int                    nbr;         // nbr of moves left to mate
};
static MATING mating;

// The list of repetition moves to avoid, normally empty
static std::vector<thc::Move> the_repetition_moves;

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
static int log( const char *fmt, ... );
static bool run_sargon( int plymax, bool avoid_book );
static std::string generate_progress_report( bool &we_are_forcing_mate, bool &we_are_stalemating_now );
static thc::Move calculate_next_move( bool new_game, unsigned long ms_time, unsigned long ms_inc, int depth );
static bool repetition_calculate( thc::ChessRules &cr, std::vector<thc::Move> &repetition_moves );
static bool test_whether_move_repeats( thc::ChessRules &cr, thc::Move mv );
static void repetition_remove_moves( const std::vector<thc::Move> &repetition_moves );
static bool repetition_test();

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
static void timer_clear();          // Clear the timer
static void timer_end();            // End the timer subsystem system
static void timer_set( int ms );    // Set a timeout event, ms millisecs into the future (0 and -1 are special values)

// main()
int main( int argc, char *argv[] )
{
    //logfile_name = std::string(argv[0]) + "-log.txt"; // wake this up for early logging
#ifdef _DEBUG
    static const std::vector<std::string> test_sequence =
    {
#if 1
        "position fen 7K/5k1P/8/8/8/8/8/8 b - - 0 1\n",
        "go depth wtime 1000 btime 1000 winc 1000 binc 5000\n",
        "isready\n",
        "quit\n"
#endif
#if 0
        "position startpos moves e2e4 c7c5 b1c3 a7a6 d1h5 e7e6 g1f3 b7b5 f3e5 g7g6 h5f3 g8f6 f1e2 f8g7 e5d3 d7d6 e4e5 f6d5 c3d5 e6d5 f3d5 a8a7 e5d6 e8g8 d5c5 a7d7 a2a4 d7d6 a4b5 a6b5 c5b5 c8d7 b5c5 d7c6 e1g1 f8e8 e2f3 c6f3 g2f3 d6d5 c5c4 d5d4 c4b5 d4d5 b5a4 d5d4 a4a5 d4d5 a5d8 e8d8 f1e1 b8c6 a1a3 d5g5 g1h1 g5h5 h1g2 d8d5 e1e8 g7f8 a3a8 d5g5 g2h1 c6e5 e8f8 g8g7 f3f4 h5h2 h1h2 e5f3 h2h3 f3g1 h3h4 g1f3 h4h3 f3g1 h3h2 g1f3\n",
        "go depth 5 wtime 88428 btime 883250 winc 1000 binc 5000\n",
        "isready\n",
        "quit\n"
#endif
#if 0
        "position startpos moves e2e4 c7c5 b1c3 a7a6 d1h5 e7e6 g1f3 b7b5 f3e5 g7g6 h5f3 g8f6 f1e2 f8g7 e5d3 d7d6 e4e5 f6d5 c3d5 e6d5 f3d5 a8a7 e5d6 e8g8 d5c5 a7d7 a2a4 d7d6 a4b5 a6b5 c5b5 c8d7 b5c5 d7c6 e1g1 f8e8 e2f3 c6f3 g2f3 d6d5 c5c4 d5d4 c4b5 d4d5 b5a4 d5d4 a4a5 d4d5 a5d8 e8d8 f1e1 b8c6 a1a3 d5g5 g1h1 g5h5 h1g2 d8d5 e1e8 g7f8 a3a8 d5g5 g2h1 c6e5 e8f8 g8g7 f3f4 h5h2 h1h2 e5f3 h2h3 f3g1 h3h4 g1f3\n",
        "go depth 5 wtime 55068 btime 889329 winc 1000 binc 5000\n",
        "isready\n",
        "quit\n"
#endif
#if 0
        "position fen 1br4k/6pp/1q6/1r4N1/8/7P/Q5P1/7K w - - 0 1\n",
        "go wtime 3599100 btime 900000 winc 2000 binc 5000\n",
        "isready\n",
        "position fen 1br4k/6pp/1q6/1r4N1/8/7P/Q5P1/7K w - - 0 1 moves g5f7 h8g8\n",
        "go wtime 3569100 btime 899203 winc 2000 binc 5000\n",
        "isready\n",
        "position fen 1br4k/6pp/1q6/1r4N1/8/7P/Q5P1/7K w - - 0 1 moves g5f7 h8g8 f7h6 g8h8\n",
        "go wtime 3568100 btime 901125 winc 2000 binc 5000\n",
        "isready\n",
        "position fen 1br4k/6pp/1q6/1r4N1/8/7P/Q5P1/7K w - - 0 1 moves g5f7 h8g8 f7h6 g8h8 a2g8 c8g8\n",
        "go wtime 3569490 btime 903235 winc 2000 binc 5000\n",
        "isready\n",
        "quit\n"
#endif
#if 0
        "isready\n",
        "position fen rnbqkb1r/pppppppp/5n2/8/2P5/5N2/PP1PPPPP/RNBQKB1R b KQkq c3 0 2 moves c7c6 d2d4 d7d5 b1c3 d5c4 a2a4 b8a6 e2e4 c8g4 f1c4 e7e6 c1e3 f8b4 e4e5 f6d5 d1c2 g4f5 c2d2 e8g8 e1g1 d5e3 f2e3 b4c3 b2c3 a6c7 a1e1 b7b5 a4b5 c6b5 c4d3 f5d3 d2d3 f7f5 e3e4 f5e4 d3e4 c7d5 e4d3 a8c8 d3b5 d5c3 b5a6 d8d7 f3g5 d7d4 g1h1 f8f1 e1f1 d4c4 a6e6 c4e6 g5e6 c8e8 e6c5 e8e5 c5d3 e5d5 d3b4 d5d6 h2h3 a7a5 b4c2 d6d2 c2e3 d2d4 f1c1 d4d3 e3c4 a5a4 c4b6 a4a3 b6c4 a3a2 c1a1 d3d1 a1d1 c3d1 h1h2 a2a1q c4d2 a1e5 h2g1 e5d4 g1h2 d4d2 h2g3 d2f2 g3g4 d1e3 g4g5 f2f5 g5h4 g7g5 h4h5 g5g4 h5h4 g8g7 h4g3 g4h3 g2h3\n",
        "go depth 6\n",
        "isready\n",
        "position fen rnbqkb1r/pp1ppppp/2p2n2/8/2PP4/5N2/PP2PPPP/RNBQKB1R b KQkq d3 0 3 moves d7d5 b1c3 d5c4 a2a4 b8a6 e2e4 c8g4 f1c4 e7e6 c1e3 f8b4 e4e5 f6d5 d1c2 g4f5 c2d2 e8g8 e1g1 d5e3 f2e3 b4c3 b2c3 a6c7 a1e1 b7b5 a4b5 c6b5 c4d3 f5d3 d2d3 f7f5 e3e4 f5e4 d3e4 c7d5 e4d3 a8c8 d3b5 d5c3 b5a6 d8d7 f3g5 d7d4 g1h1 f8f1 e1f1 d4c4 a6e6 c4e6 g5e6 c8e8 e6c5 e8e5 c5d3 e5d5 d3b4 d5d6 h2h3 a7a5 b4c2 d6d2 c2e3 d2d4 f1c1 d4d3 e3c4 a5a4 c4b6 a4a3 b6c4 a3a2 c1a1 d3d1 a1d1 c3d1 h1h2 a2a1q c4d2 a1e5 h2g1 e5d4 g1h2 d4d2 h2g3 d2f2 g3g4 d1e3 g4g5 f2f5 g5h4 g7g5 h4h5 g5g4 h5h4 g8g7 h4g3 g4h3 g2h3 h7h5 g3h2\n",
        "go depth 7\n",
        "isready\n",
        "position fen rnbqkb1r/pp2pppp/2p2n2/3p4/2PP4/2N2N2/PP2PPPP/R1BQKB1R b KQkq - 1 4 moves d5c4 a2a4 b8a6 e2e4 c8g4 f1c4 e7e6 c1e3 f8b4 e4e5 f6d5 d1c2 g4f5 c2d2 e8g8 e1g1 d5e3 f2e3 b4c3 b2c3 a6c7 a1e1 b7b5 a4b5 c6b5 c4d3 f5d3 d2d3 f7f5 e3e4 f5e4 d3e4 c7d5 e4d3 a8c8 d3b5 d5c3 b5a6 d8d7 f3g5 d7d4 g1h1 f8f1 e1f1 d4c4 a6e6 c4e6 g5e6 c8e8 e6c5 e8e5 c5d3 e5d5 d3b4 d5d6 h2h3 a7a5 b4c2 d6d2 c2e3 d2d4 f1c1 d4d3 e3c4 a5a4 c4b6 a4a3 b6c4 a3a2 c1a1 d3d1 a1d1 c3d1 h1h2 a2a1q c4d2 a1e5 h2g1 e5d4 g1h2 d4d2 h2g3 d2f2 g3g4 d1e3 g4g5 f2f5 g5h4 g7g5 h4h5 g5g4 h5h4 g8g7 h4g3 g4h3 g2h3 h7h5 g3h2 f5e5 h2g1\n",
        "go depth 7\n",
        "isready\n",
        "position fen rnbqkb1r/pp2pppp/2p2n2/8/P1pP4/2N2N2/1P2PPPP/R1BQKB1R b KQkq a3 0 5 moves b8a6 e2e4 c8g4 f1c4 e7e6 c1e3 f8b4 e4e5 f6d5 d1c2 g4f5 c2d2 e8g8 e1g1 d5e3 f2e3 b4c3 b2c3 a6c7 a1e1 b7b5 a4b5 c6b5 c4d3 f5d3 d2d3 f7f5 e3e4 f5e4 d3e4 c7d5 e4d3 a8c8 d3b5 d5c3 b5a6 d8d7 f3g5 d7d4 g1h1 f8f1 e1f1 d4c4 a6e6 c4e6 g5e6 c8e8 e6c5 e8e5 c5d3 e5d5 d3b4 d5d6 h2h3 a7a5 b4c2 d6d2 c2e3 d2d4 f1c1 d4d3 e3c4 a5a4 c4b6 a4a3 b6c4 a3a2 c1a1 d3d1 a1d1 c3d1 h1h2 a2a1q c4d2 a1e5 h2g1 e5d4 g1h2 d4d2 h2g3 d2f2 g3g4 d1e3 g4g5 f2f5 g5h4 g7g5 h4h5 g5g4 h5h4 g8g7 h4g3 g4h3 g2h3 h7h5 g3h2 f5e5 h2g1 e5f5 g1h2\n",
        "go depth 7\n",
        "isready\n",
        "position fen r1bqkb1r/pp2pppp/n1p2n2/8/P1pPP3/2N2N2/1P3PPP/R1BQKB1R b KQkq e3 0 6 moves c8g4 f1c4 e7e6 c1e3 f8b4 e4e5 f6d5 d1c2 g4f5 c2d2 e8g8 e1g1 d5e3 f2e3 b4c3 b2c3 a6c7 a1e1 b7b5 a4b5 c6b5 c4d3 f5d3 d2d3 f7f5 e3e4 f5e4 d3e4 c7d5 e4d3 a8c8 d3b5 d5c3 b5a6 d8d7 f3g5 d7d4 g1h1 f8f1 e1f1 d4c4 a6e6 c4e6 g5e6 c8e8 e6c5 e8e5 c5d3 e5d5 d3b4 d5d6 h2h3 a7a5 b4c2 d6d2 c2e3 d2d4 f1c1 d4d3 e3c4 a5a4 c4b6 a4a3 b6c4 a3a2 c1a1 d3d1 a1d1 c3d1 h1h2 a2a1q c4d2 a1e5 h2g1 e5d4 g1h2 d4d2 h2g3 d2f2 g3g4 d1e3 g4g5 f2f5 g5h4 g7g5 h4h5 g5g4 h5h4 g8g7 h4g3 g4h3 g2h3 h7h5 g3h2 f5e5 h2g1 e5f5 g1h2 f5e5 h2g1\n",
        "go depth 7\n",
        "isready\n",
        "quit\n"
#endif
#if 0
        "uci\n",
        "isready\n",
        "setoption name FixedDepth value 1\n",           // go straight to depth 1 without iterating
        "setoption name LogFileName value c:\\windows\\temp\\sargon-log-file.txt\n",
        "position fen 7k/2pp2pp/4q3/4b3/4R3/4Q3/6PP/7K w - - 0 1\n",   // can take the bishop 425 centipawns
        "go\n",
        "position fen 7k/2pp2pp/4q3/4b3/4R3/3Q4/6PP/7K w - - 0 1\n",   // can't take the bishop 175 centipawns 
        "go\n"
#endif
#if 0
        "uci\n",
        "isready\n",
        "setoption name FixedDepth value 5\n",           // go straight to depth 5 without iterating
        "setoption name LogFileName value c:\\windows\\temp\\sargon-log-file.txt\n",
        "position fen 7k/8/8/8/8/8/8/N6K b - - 0 1\n",   // an extra knight
        "go depth 6\n",                                  // will override fixed depth option, iterates
        "position fen 7k/8/8/8/8/8/8/N5Kq w - - 0 1\n",  // an extra knight only after capturing queen
        "go\n"                                           // will go straight to depth 5, no iteration
#endif
    };
    //bool ok = repetition_test();
    //printf( "repetition test %s\n", ok?"passed":"failed" );
    for( std::string s: test_sequence )
    {
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

    // Tell timer thread to finish, then kill it immediately
    timer_end();
    third.detach();
    return 0;
}

// Emulate the simple and very useful Windows GetTickCount() with more elaborate and flexible C++
//  std library calls, at least we can give our function a better name that the misleading
//  GetTickCount() which fails to mention milliseconds in its name
static std::chrono::time_point<std::chrono::steady_clock> base = std::chrono::steady_clock::now();
static unsigned long elapsed_milliseconds()
{
    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - base);
    unsigned long ret = static_cast<unsigned long>(ms.count());
    return ret;
}

// Very simple timer thread, controlled by timer_set(), timer_clear(), timer_end() 
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
static void timer_clear()
{
    timer_set(0);  // set sentinel value 0
}

// End the timer subsystem system
static void timer_end()
{
    timer_set(-1);  // set sentinel value -1
}

// Set a timeout event, ms millisecs into the future (0 and -1 are special values)
static void timer_set( int ms )
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

    // Check for timer_end()
    if( ms == -1 )
        future_time = -1;

    // Schedule a new "TIMEOUT" event (unless 0 = timer_clear())
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
static bool run_sargon( int plymax, bool avoid_book )
{
    bool aborted = false;
    int val;
    val = setjmp(jmp_buf_env);
    if( val )
        aborted = true;
    else
        sargon_run_engine(the_position,plymax,the_pv,avoid_book); // the_pv updated only if not aborted
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
         "genmov callbacks=%lu\n"
         "end of points callbacks=%lu\n",
            cmd.c_str(),
            total_callbacks,
            bestmove_callbacks,
            genmov_callbacks,
            end_of_points_callbacks );
    log( "%s\n", sargon_pv_report_stats().c_str() );
    return quit;
}

static std::string cmd_uci()
{
    std::string rsp=
    "id name " ENGINE_NAME " " VERSION "\n"
    "id author Dan and Kathe Spracklin, Windows port by Bill Forster\n"
    "option name FixedDepth type spin min 0 max 20 default 0\n"
    "option name LogFileName type string default\n"
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
    std::string ret = stop_rsp;
    stop_rsp.clear();
    return ret;
}

static void cmd_setoption( const std::vector<std::string> &fields )
{
    // Option "FixedDepth"
    //  Range is 0-20, default is 0. 0 indicates auto depth selection,
    //   others are fixed depth
    // eg "setoption name FixedDepth value 3"
    if( fields.size()>4 && fields[1]=="name" && fields[2]=="fixeddepth" && fields[3]=="value" )
    {
        depth_option = atoi(fields[4].c_str());
        if( depth_option<0 || depth_option>20 )
            depth_option = 0;
    }

    // Option "LogFileName"
    //   string, default is empty string (no log kept in that case)
    // eg "setoption name LogFileName value c:\windows\temp\sargon-log-file.txt"
    else if( fields.size()>4 && fields[1]=="name" && fields[2]=="logfilename" && fields[3]=="value" )
    {
        logfile_name = fields[4];
    }
}

static std::string cmd_go( const std::vector<std::string> &fields )
{
    the_pv.clear();
    stop_rsp = "";
    base_time = elapsed_milliseconds();
    total_callbacks = 0;
    bestmove_callbacks = 0;
    genmov_callbacks = 0;
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
    bool expecting_depth = false;
    int ms_time   = 0;
    int ms_inc    = 0;
    int depth     = 0;
    for( std::string parm: fields )
    {
        if( expecting_time )
        {
            ms_time = atoi(parm.c_str());
            expecting_time = false;
        }
        else if( expecting_inc )
        {
            ms_inc = atoi(parm.c_str());
            expecting_inc = false;
        }
        else if( expecting_depth )
        {
            depth = atoi(parm.c_str());
            expecting_depth = false;
        }
        else
        {
            if( parm == stime )
                expecting_time = true;
            if( parm == sinc )
                expecting_inc = true;
            if( parm == "depth" )
                expecting_depth = true;
        }
    }
    bool new_game = is_new_game();
    thc::Move bestmove = calculate_next_move( new_game, ms_time, ms_inc, depth );
    return util::sprintf( "bestmove %s\n", bestmove.TerseOut().c_str() );
}

static void cmd_go_infinite()
{
    the_pv.clear();
    stop_rsp = "";
    int plymax=1;
    bool aborted = false;
    base_time = elapsed_milliseconds();
    total_callbacks = 0;
    bestmove_callbacks = 0;
    genmov_callbacks = 0;
    end_of_points_callbacks = 0;
    while( !aborted )
    {
        aborted = run_sargon(plymax,true);  // note avoid_book = true
        if( plymax < 20 )
            plymax++;
        if( !aborted )
        {
            bool we_are_forcing_mate, we_are_stalemating_now;
            std::string out = generate_progress_report( we_are_forcing_mate, we_are_stalemating_now );
            if( out.length() > 0 )
            {
                fprintf( stdout, out.c_str() );
                fflush( stdout );
                log( "rsp>%s\n", out.c_str() );
                stop_rsp = util::sprintf( "bestmove %s\n", the_pv.variation[0].TerseOut().c_str() ); 
            }
        }
    }
    if( stop_rsp == "" )    // Shouldn't actually ever happen as callback polling doesn't abort
    {                       //  run_sargon() if plymax is 1
        run_sargon(1,false);
        std::string bestmove = sargon_export_move(BESTM);
        stop_rsp = util::sprintf( "bestmove %s\n", bestmove.c_str() ); 
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

// Return true if PV has us (the engine) forcing mate
static std::string generate_progress_report( bool &we_are_forcing_mate, bool &we_are_stalemating_now )
{
    we_are_forcing_mate    = false;
    we_are_stalemating_now = false;
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
                if( i == 0 )
                    we_are_stalemating_now = true;
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
            {
                mating.active    = true;
                mating.position  = the_position;
                mating.variation = the_pv.variation;
                mating.idx       = 0;
                mating.nbr       = score_overide;
                buf_score = util::sprintf( "mate %d", mating.nbr );
                we_are_forcing_mate = true;
                the_pv.value = 120;
            }
            else if( score_overide < 0 ) // are me being mated ?
            {
                buf_score = util::sprintf( "mate -%d", (0-score_overide) );
                the_pv.value = -120;
            }
            else if( score_overide == 0 ) // is it a stalemate draw ?
            {
                buf_score = util::sprintf( "cp 0" );
                the_pv.value = 0;
            }
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
            {
                mating.active    = true;
                mating.position  = the_position;
                mating.variation = the_pv.variation;
                mating.idx       = 0;
                mating.nbr       = 0-score_overide;
                buf_score = util::sprintf( "mate %d", mating.nbr );
                we_are_forcing_mate = true;
                the_pv.value = -120;
            }
            else if( score_overide > 0 ) // are me being mated ?        
            {
                buf_score = util::sprintf( "mate -%d", score_overide );
                the_pv.value = 120;
            }
            else if( score_overide == 0 ) // is it a stalemate draw ?
            {
                buf_score = util::sprintf( "cp 0" );
                the_pv.value = 0;
            }
        }
        else
        {
            buf_score = util::sprintf( "cp %d", 0-score_cp );
        }
    }
    std::string out;
    if( the_pv.variation.size() > 0 )
    {
        out = util::sprintf( "info depth %d score %s time %lu nodes %lu nps %lu pv%s\n",
                    depth,
                    buf_score.c_str(),
                    (unsigned long) elapsed_time,
                    (unsigned long) nodes,
                    1000L * ((unsigned long) nodes / (unsigned long)elapsed_time ),
                    buf_pv.c_str() );
    }
    return out;
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
        set cutoff timer to MEDIUM time, establishes target
      else
        set cutoff timer to HIGH time, loop to target
        if hit target in less that LOW time
          increment target and loop again
        if we are cut
          decrement target

     HOWEVER: We don't always loop, and we don't always use the timer

     1) Once a PV leading to Sargon mating is calculated, play out the mate
        without further ado, always. Terminate looping on discovery and
        don't loop as the moves are played out. If opponent deviates from
        the mating PV though, normal service is resumed. (But see 3)).
     2) In fixed depth mode, (either the FixedDepth engine option is set
        or the depth parameter to the go command is set, don't loop and don't
        use the timer.
     3) Exception to 2), if normal service is resumed after Sargon has
        identified a forced mate always loop.
     4) Repetition avoidance: If Sargon repeats the position when it thinks
        it's better, loop once more (even if we aren't otherwise looping) in
        a special mode with all repeating moves excluded. Play the new best
        move or fallback if we are worse. Use one lower depth unless we are
        using fixed depth.


     Pseudo code

     States

     ADAPTIVE_NO_TARGET_YET
     ADAPTIVE_WITH_TARGET
     FIXED
     FIXED_WITH_LOOPING
     PLAYING_OUT_MATE_ADAPTIVE
     PLAYING_OUT_MATE_FIXED
     REPEATING_ADAPTIVE
     REPEATING_FIXED
     REPEATING_FIXED_WITH_LOOPING

     if new_game
         calculate initial state
     state_machine_initial
     loop
         aborted,elapsed,mating,pv = run sargon
         if mating
            state = PLAYING_OUT_MATE_ADAPTIVE or PLAYING_OUT_MATE_FIXED
            return mating_move
         ready = state_machine_loop(aborted,elapsed,pv)
         if ready and repeats and we are better
             state = REPEATING_ADAPTIVE or REPEATING_FIXED or REPEATING_FIXED_WITH_LOOPING
             if state == REPEATING_ADAPTIVE
                level--
                clear timer
             not ready
         if ready
            return best move

     state_machine_initial:
     PLAYING_OUT_MATE_ADAPTIVE
        if opponent follows line
            play move and exit
        else
            state = ADAPTIVE_WITH_NO_TARGET_YET fall through
     ADAPTIVE_NO_TARGET_YET
        set cutoff timer to MEDIUM time
        plymax = 1
     PLAYING_OUT_MATE_FIXED
        if opponent follows line
            play move and exit
        else
            state = FIXED_WITH_LOOPING fall through
     FIXED_WITH_LOOPING
        plymax  = 1
        target = FIXED_DEPTH
     ADAPTIVE_WITH_TARGET
        set cutoff timer to HIGH time
        plymax = 1
     FIXED
        plymax = 1,2,3 then FIXED_DEPTH
        (1,2 and 3 are almost instantaneous and will pick up quick mates)
     REPEATING_ADAPTIVE
     REPEATING_FIXED
     REPEATING_FIXED_WITH_LOOPING

     state_machine_loop:
     ADAPTIVE_NO_TARGET_YET
        if aborted
            target = plymax-1
            state = ADAPTIVE_WITH_TARGET
            return ready
        else
            plymax++
            return not ready
     ADAPTIVE_WITH_TARGET
        if aborted
            target = plymax-1
            return ready
        else if plymax < target
            plymax++
            return not ready
        else if plymax >= target && elapsed < lo_timer
            plymax++
            target++
            return not ready
        else
            return ready
     FIXED
        set plymax = 1,2,3 then FIXED_DEPTH
        return ready after FIXED_DEPTH
     FIXED_WITH_LOOPING
        if plymax >= target
            state = FIXED (forcing mate hasn't worked)
            return ready
        else
            plymax++
            return not ready
     REPEATING_ADAPTIVE
        if we are not better
            revert
        state = ADAPTIVE_WITH_TARGET
        return ready
     REPEATING_FIXED
        if we are not better
            revert
        state = FIXED
        return ready
     REPEATING_FIXED_WITH_LOOPING
        if we are not better
            revert
        state = FIXED_WITH_LOOPING
        return ready

*/

// States
enum PlayingState
{
    ADAPTIVE_NO_TARGET_YET,
    ADAPTIVE_WITH_TARGET,
    FIXED,
    FIXED_WITH_LOOPING,
    PLAYING_OUT_MATE_ADAPTIVE,
    PLAYING_OUT_MATE_FIXED,
    REPEATING_ADAPTIVE,
    REPEATING_FIXED,
    REPEATING_FIXED_WITH_LOOPING
};

static void log_state_changes( const std::string &msg, PlayingState old_state, PlayingState state )
{
    const char *txt=NULL, *old_txt=NULL, *new_txt=NULL;
    for( int i=0; i<2; i++ )
    {
        PlayingState temp = (i==0 ? old_state : state );
        switch( temp )
        {
            case ADAPTIVE_NO_TARGET_YET:        txt = "ADAPTIVE_NO_TARGET_YET";       break;
            case ADAPTIVE_WITH_TARGET:          txt = "ADAPTIVE_WITH_TARGET";         break;
            case FIXED:                         txt = "FIXED";                        break;
            case FIXED_WITH_LOOPING:            txt = "FIXED_WITH_LOOPING";           break;
            case PLAYING_OUT_MATE_ADAPTIVE:     txt = "PLAYING_OUT_MATE_ADAPTIVE";    break;
            case PLAYING_OUT_MATE_FIXED:        txt = "PLAYING_OUT_MATE_FIXED";       break;
            case REPEATING_ADAPTIVE:            txt = "REPEATING_ADAPTIVE";           break;
            case REPEATING_FIXED:               txt = "REPEATING_FIXED";              break;
            case REPEATING_FIXED_WITH_LOOPING:  txt = "REPEATING_FIXED_WITH_LOOPING"; break;
        }
        if( i == 0 )
            old_txt = txt;
        else
            new_txt = txt;
    }
    if( state == old_state )
        log( "%s, state = %s\n", msg.c_str(), old_txt );
    else
        log( "%s, state = %s -> %s\n", msg.c_str(), old_txt, new_txt );
}

static thc::Move calculate_next_move( bool new_game, unsigned long ms_time, unsigned long ms_inc, int depth )
{
    // Timers
    const unsigned long LO =100;
    const unsigned long MED=30;
    const unsigned long HI =16;
    unsigned long ms_lo  = ms_time / LO;
    unsigned long ms_med = ms_time / MED;
    unsigned long ms_hi  = ms_time / HI;
    bool timer_running = false;

    // States
    static PlayingState state = ADAPTIVE_NO_TARGET_YET;
    PlayingState old_state;

    // Misc
    static int plymax_target;
    int plymax = 1;
    int stalemates = 0;
    unsigned long base = elapsed_milliseconds();
    PV repetition_fallback_pv;
    the_repetition_moves.clear();

    //  if new_game
    //     calculate initial state
    if( depth_option > 0 )
        depth = depth_option;
    if( new_game )
    {
        state = depth>0 ? FIXED : ADAPTIVE_NO_TARGET_YET;
    }

    // else sanity check on states
    // (sadly new_game == false is not entirely reliable - eg if we run
    //  kibiter before starting game new_game will be false, even though
    //  for our purposes it is really true)
    else
    {
        if( depth > 0 )
        {
            switch( state )
            {
                case ADAPTIVE_NO_TARGET_YET:        state = FIXED;  break;
                case ADAPTIVE_WITH_TARGET:          state = FIXED;  break;
                case REPEATING_ADAPTIVE:            state = FIXED;  break;
                case REPEATING_FIXED:               state = FIXED;  break;
                case REPEATING_FIXED_WITH_LOOPING:  state = FIXED;  break;
            }
        }
        else
        {
            switch( state )
            {
                case FIXED:                         state = ADAPTIVE_NO_TARGET_YET; break;
                case FIXED_WITH_LOOPING:            state = ADAPTIVE_NO_TARGET_YET; break;
                case REPEATING_ADAPTIVE:            state = ADAPTIVE_NO_TARGET_YET; break;
                case REPEATING_FIXED:               state = ADAPTIVE_NO_TARGET_YET; break;
                case REPEATING_FIXED_WITH_LOOPING:  state = ADAPTIVE_NO_TARGET_YET; break;
            }
        }
    }

    // Initial state machine
    old_state = state;
    switch( state )
    {
        //  PLAYING_OUT_MATE_ADAPTIVE
        //      if opponent follows line
        //          play move and exit
        //      else
        //          state = ADAPTIVE_WITH_NO_TARGET_YET fall through
        //  PLAYING_OUT_MATE_FIXED
        //      if opponent follows line
        //          play move and exit
        //      else
        //          state = FIXED_WITH_LOOPING fall through
        case PLAYING_OUT_MATE_ADAPTIVE:
        case PLAYING_OUT_MATE_FIXED:
        {
            // Play out a mating sequence
            bool opponent_follows_line = false;
            if( mating.active && mating.idx+2 < mating.variation.size() )
            {
                mating.position.PlayMove(mating.variation[mating.idx++]);
                mating.position.PlayMove(mating.variation[mating.idx++]);
                if( mating.position == the_position )
                    opponent_follows_line = true;
            }
            if( opponent_follows_line )
            {
                thc::ChessRules cr = mating.position;
                thc::Move mating_move = mating.variation[mating.idx];
                std::string buf_pv;
                for( unsigned int i=0; mating.idx+i<mating.variation.size(); i++ )
                {
                    thc::Move move=mating.variation[mating.idx+i];
                    cr.PlayMove( move );
                    buf_pv += " ";
                    buf_pv += move.TerseOut();
                }
                std::string out = util::sprintf( "info score mate %d pv%s\n",
                    --mating.nbr,
                    buf_pv.c_str() );
                if( mating.nbr <= 1 )
                {
                    mating.active = false;
                    state = (state==PLAYING_OUT_MATE_ADAPTIVE ? ADAPTIVE_NO_TARGET_YET : FIXED);
                }
                fprintf( stdout, out.c_str() );
                fflush( stdout );
                log( "rsp>%s\n", out.c_str() );
                log( "(%s mating line)\n", mating.nbr<=1 ? "Finishing" : "Continuing" );
                stop_rsp = util::sprintf( "bestmove %s\n", mating_move.TerseOut().c_str() ); 
                return mating_move;
            }
            else
            {
                // If the opponent chooses an alternative path, we still have forced mate (if we trust
                //  Sargon's fundamental chess algorithms), so used FIXED_WITH_LOOPING rather than
                //  FIXED, in order to find it quickly and efficiently (this is why we have
                //  FIXED_WITH_LOOPING)
                mating.active = false;
                state = (state==PLAYING_OUT_MATE_ADAPTIVE ? ADAPTIVE_NO_TARGET_YET : FIXED_WITH_LOOPING);

                // Simulate fall through to ADAPTIVE_NO_TARGET_YET / FIXED_WITH_LOOPING
                if( state == ADAPTIVE_NO_TARGET_YET )
                {
                    timer_set( ms_med );
                    timer_running = true;
                    plymax = 1;
                }
                else // if( state == FIXED_WITH_LOOPING )
                {
                    plymax_target = depth;
                    plymax  = 1;
                }
            }
            break;
        }

        //  ADAPTIVE_NO_TARGET_YET
        //      set cutoff timer to MEDIUM time
        //      plymax = 1
        case ADAPTIVE_NO_TARGET_YET:
        {
            timer_set( ms_med );
            timer_running = true;
            plymax = 1;
            break;
        }

        //  ADAPTIVE_WITH_TARGET
        //      set cutoff timer to HIGH time
        //      plymax = 1
        case ADAPTIVE_WITH_TARGET:
        {
            timer_set( ms_hi );
            timer_running = true;
            plymax = 1;
            break;
        }

        //  FIXED_WITH_LOOPING
        //      plymax  = 1
        //      target = FIXED_DEPTH
        case FIXED_WITH_LOOPING:
        {
            plymax = 1;
            plymax_target = depth;
            break;
        }

        //  FIXED
        //      plymax = 1,2,3 then FIXED_DEPTH
        //      (1,2 and 3 are almost instantaneous and will pick up quick mates)
        case FIXED:
        {
            plymax = 1;
            break;
        }
    }   // end switch
    log_state_changes( "Initial state machine:", old_state, state );

    //  loop
    //      aborted,elapsed,mating,pv = run sargon
    bool we_are_stalemating_now = false;
    //bool just_once = true;
    for(;;)
    {
        bool aborted = run_sargon(plymax,false);
        unsigned long now = elapsed_milliseconds();
        unsigned long elapsed = (now-base);

        // The special case, where Sargon minimax never ran should only be book move
        if( the_pv.variation.size() == 0 )    
        {
            std::string bestmove_terse = sargon_export_move(BESTM);
            thc::Move bestmove;
            bestmove.Invalid();
            bool have_move = bestmove.TerseIn( &the_position, bestmove_terse.c_str() );
            if( have_move )
                log( "No PV, expect Sargon is playing book move - %s\n%s", bestmove_terse.c_str(), the_position.ToDebugStr().c_str() );
            else
                log( "Sargon doesn't find move - %s\n%s", bestmove_terse.c_str(), the_position.ToDebugStr().c_str() );
            stop_rsp = util::sprintf( "bestmove %s\n", bestmove_terse.c_str() );
            if( timer_running )
                timer_clear();
            return bestmove;
        }

        // Report on each normally concluded iteration
        bool we_are_forcing_mate = false;
        std::string info;
        if( aborted )
        {
            log( "aborted=%s, elapsed=%lu, ms_lo=%lu, plymax=%d, plymax_target=%d\n",
                    aborted?"true @@":"false", //@@ marks move in log
                    elapsed, ms_lo, plymax, plymax_target );
        }
        else
        {
            std::string s = the_pv.variation[0].TerseOut();
            std::string bestm = sargon_export_move(BESTM);
            if( s.substr(0,4) != bestm )
                log( "Unexpected event: BESTM=%s != PV[0]=%s\n%s", bestm.c_str(), s.c_str(), the_position.ToDebugStr().c_str() );
            info = generate_progress_report( we_are_forcing_mate, we_are_stalemating_now );
            bool repeating = (state==REPEATING_ADAPTIVE || state==REPEATING_FIXED || state==REPEATING_FIXED_WITH_LOOPING);
            if( (!repeating||we_are_forcing_mate) && info.length() > 0 )
            {
                fprintf( stdout, info.c_str() );
                fflush( stdout );
                log( "rsp>%s\n", info.c_str() );
                stop_rsp = util::sprintf( "bestmove %s\n", the_pv.variation[0].TerseOut().c_str() ); 
                info.clear();
            }
        }

        //  if mating
        //     state = PLAYING_OUT_MATE_ADAPTIVE or PLAYING_OUT_MATE_FIXED
        //     return mating_move
        if( we_are_forcing_mate )
        {
            if( mating.variation.size() == 1 )
            {
                log( "(Immediate mate in one available, play it)\n" );
                mating.active = false;
            }
            else
            {
                log( "(Starting mating line)\n" );
                switch(state)
                {
                    case ADAPTIVE_NO_TARGET_YET:        state = PLAYING_OUT_MATE_ADAPTIVE;  break;
                    case ADAPTIVE_WITH_TARGET:          state = PLAYING_OUT_MATE_ADAPTIVE;  break;
                    case FIXED:                         state = PLAYING_OUT_MATE_FIXED;     break;
                    case FIXED_WITH_LOOPING:            state = PLAYING_OUT_MATE_FIXED;     break;
                    case PLAYING_OUT_MATE_ADAPTIVE:     state = PLAYING_OUT_MATE_ADAPTIVE;  break;
                    case PLAYING_OUT_MATE_FIXED:        state = PLAYING_OUT_MATE_FIXED;     break;
                    case REPEATING_ADAPTIVE:            state = PLAYING_OUT_MATE_ADAPTIVE;  break;
                    case REPEATING_FIXED:               state = PLAYING_OUT_MATE_FIXED;     break;
                    case REPEATING_FIXED_WITH_LOOPING:  state = PLAYING_OUT_MATE_FIXED;     break;
                }
            }
            if( timer_running )
                timer_clear();
            return mating.variation[mating.idx];
        }

        // ready = state_machine_loop(aborted,elapsed,pv)
        bool ready = false;
        bool after_repetition_avoidance = false;

        // Loop state machine
        old_state = state;
        switch(state)
        {
            //  ADAPTIVE_NO_TARGET_YET
            //      if aborted
            //          target = plymax-1
            //          state = ADAPTIVE_WITH_TARGET
            //          return ready
            //      else
            //          plymax++
            //          return not ready
            case ADAPTIVE_NO_TARGET_YET:
            {
                if( aborted )
                {
                    plymax_target = (plymax>=2 ? plymax-1 : 1);
                    state  = ADAPTIVE_WITH_TARGET;
                    ready = true;
                }
                else
                {
                    plymax++;
                }
                break;
            }

            //  ADAPTIVE_WITH_TARGET
            //      if aborted
            //          target = plymax-1
            //          return ready
            //      else if plymax < target
            //          plymax++
            //          return not ready
            //      else if plymax >= target && elapsed < lo_timer
            //          plymax++
            //          target++
            //          return not ready
            //      else
            //          return ready
            case ADAPTIVE_WITH_TARGET:          
            {
                if( !aborted && plymax>=plymax_target )
                    log( "Reached adaptive target, increase target if elapsed=%lu < ms_lo=%lu\n", elapsed, ms_lo );
                if( aborted )
                {
                    plymax_target = (plymax>=2 ? plymax-1 : 1);
                    ready = true;
                }
                else if( plymax < plymax_target )
                {
                    plymax++;
                }
                else if( plymax>=plymax_target && elapsed<ms_lo )
                {
                    plymax++;
                    plymax_target++;
                    log( "Increase adaptive target\n" );
                }
                //else if( plymax>=plymax_target && we_are_stalemating_now && just_once )
                //{
                //    just_once = false;
                //    plymax++;  // try one higher depth if we are stalemating
                //}
                else
                {
                    ready = true;
                }
                break;
            }

            //  FIXED
            //      set plymax = 1,2,3 then FIXED_DEPTH
            //      return ready after FIXED_DEPTH
            case FIXED:                         
            {
                if( 1<=plymax && plymax<3 && plymax<depth )
                {
                    plymax++;
                }
                else if( plymax < depth )
                {
                    plymax = depth;
                }
                else
                {
                    ready = true;
                }
                break;
            }

            //  FIXED_WITH_LOOPING
            //      if plymax >= target
            //          state = FIXED (forcing mate hasn't worked)
            //          return ready
            //      else
            //          plymax++
            //          return not ready
            case FIXED_WITH_LOOPING:            
            {
                if( plymax >= plymax_target )
                {
                    state = FIXED;
                    ready = true;
                }
                else
                {
                    plymax++;
                }
                break;
            }
            case PLAYING_OUT_MATE_ADAPTIVE:     
            {
                break;
            }
            case PLAYING_OUT_MATE_FIXED:        
            {
                break;
            }

            //  REPEATING_ADAPTIVE
            //  REPEATING_FIXED
            //  REPEATING_FIXED_WITH_LOOPING
            //      if we are not better
            //          revert
            //      state = ADAPTIVE_WITH_TARGET / FIXED / FIXED_WITH_LOOPING
            //      return ready
            case REPEATING_ADAPTIVE:            
            case REPEATING_FIXED:               
            case REPEATING_FIXED_WITH_LOOPING:  
            {
                // If attempts to avoid repetition leave us worse, fall back
                after_repetition_avoidance = true;
                bool fallback=false;
                int score_after_repetition_avoid = the_pv.value;
                if( the_position.white )
                    fallback = (score_after_repetition_avoid <= 0);
                else
                    fallback = (score_after_repetition_avoid >= 0);
                if( aborted )
                    fallback = true;
                if( fallback )
                    the_pv = repetition_fallback_pv;
                else if( info.length() > 0 )
                {
                    fprintf( stdout, info.c_str() );
                    fflush( stdout );
                    log( "rsp>%s\n", info.c_str() );
                    stop_rsp = util::sprintf( "bestmove %s\n", the_pv.variation[0].TerseOut().c_str() ); 
                    info.clear();
                }
                log( "After repetition avoidance, new value=%d, so %s\n", score_after_repetition_avoid, fallback ? "did fallback" : "didn't fallback" );
                ready = true;
                switch(state)
                {
                    case REPEATING_ADAPTIVE:            state = ADAPTIVE_WITH_TARGET;  break;
                    case REPEATING_FIXED:               state = FIXED;                 break;
                    case REPEATING_FIXED_WITH_LOOPING:  state = FIXED_WITH_LOOPING;    break;
                }
                break;
            }
        }
        log_state_changes( "Loop state machine:", old_state, state );


        //  if ready and repeats and we are better
        //      state = REPEATING_ADAPTIVE or REPEATING_FIXED or REPEATING_FIXED_WITH_LOOPING
        //      if state == REPEATING_ADAPTIVE
        //         level--
        //         clear timer
        //      not ready
        // If we are about to play move, and we're better; consider whether to kick in repetition avoidance
        if( !after_repetition_avoidance && ready && (the_position.white? the_pv.value>0 : the_pv.value<0) )
        {
            thc::Move mv = the_pv.variation[0];
            if( test_whether_move_repeats(the_position,mv) )
            {
                log( "Repetition avoidance, %s repeats\n", mv.TerseOut().c_str() );
                bool ok = repetition_calculate( the_position, the_repetition_moves );
                if( !ok )
                {
                    the_repetition_moves.clear();   // don't do repetition avoidance - all moves repeat
                }
                else
                {
                    repetition_fallback_pv = the_pv;
                    switch(state)
                    {
                        case ADAPTIVE_NO_TARGET_YET:        state = REPEATING_ADAPTIVE;             break;
                        case ADAPTIVE_WITH_TARGET:          state = REPEATING_ADAPTIVE;             break;
                        case FIXED:                         state = REPEATING_FIXED;                break;
                        case FIXED_WITH_LOOPING:            state = REPEATING_FIXED_WITH_LOOPING;   break;
                        case PLAYING_OUT_MATE_ADAPTIVE:     state = REPEATING_ADAPTIVE;             break;
                        case PLAYING_OUT_MATE_FIXED:        state = REPEATING_FIXED;                break;
                        case REPEATING_ADAPTIVE:            state = REPEATING_ADAPTIVE;             break;
                        case REPEATING_FIXED:               state = REPEATING_FIXED;                break;
                        case REPEATING_FIXED_WITH_LOOPING:  state = REPEATING_FIXED_WITH_LOOPING;   break;
                    }
                    if( state == REPEATING_ADAPTIVE )
                    {
                        if( plymax > 1 )
                            plymax--;
                        if( timer_running )
                        {
                            timer_running = false;
                            timer_clear();
                        }
                    }
                    ready = false;
                }
            }
        }

        //  if ready
        //     return best move
        if( ready )
            break;
    }
    thc::Move mv = the_pv.variation[0];
    if( timer_running )
        timer_clear();
    return mv;
}

#if 0
static thc::Move calculate_next_move( bool new_game, unsigned long ms_time, unsigned long ms_inc, int depth )
{
    // Play out a mating sequence
    if( mating.active && mating.idx+2 < mating.variation.size() )
    {
        mating.position.PlayMove(mating.variation[mating.idx++]);
        mating.position.PlayMove(mating.variation[mating.idx++]);
        if( mating.position != the_position )
            mating.active = false;
        else
        {
            thc::ChessRules cr = mating.position;
            thc::Move mating_move = mating.variation[mating.idx];
            std::string buf_pv;
            for( unsigned int i=0; mating.idx+i<mating.variation.size(); i++ )
            {
                thc::Move move=mating.variation[mating.idx+i];
                cr.PlayMove( move );
                buf_pv += " ";
                buf_pv += move.TerseOut();
            }
            std::string out = util::sprintf( "info score mate %d pv%s\n",
                --mating.nbr,
                buf_pv.c_str() );
            fprintf( stdout, out.c_str() );
            fflush( stdout );
            log( "rsp>%s\n", out.c_str() );
            stop_rsp = util::sprintf( "bestmove %s\n", mating_move.TerseOut().c_str() ); 
            return mating_move;
        }
    }

    // If not playing out a mate, start normal algorithm
    log( "Input ms_time=%lu, ms_inc=%lu, depth=%d\n", ms_time, ms_inc, depth );
    static int plymax_target;
    unsigned long ms_lo=0;
    bool go_straight_to_fixed_depth = false;
    the_repetition_moves.clear();

    // There are multiple reasons why we would run fixed depth, without a timer
    if( depth == 0 )
    {
        if( depth_option > 0 )
        {
            depth = depth_option;
            go_straight_to_fixed_depth = true;  // Only this way do we go straight to
                                                //  specified depth without iterating
        }
        else if( ms_time == 0 )
            depth = 3;  // Set plymax=3 as a baseline, it's more or less instant
    }
    bool fixed_depth = (depth>0);
    if( fixed_depth )
        timer_clear();

    // Otherwise set a cutoff timer
    else
    {
        const unsigned long LO =100;
        const unsigned long MED=30;
        const unsigned long HI =16;
        ms_lo  = ms_time / LO;
        unsigned long ms_med = ms_time / MED;
        unsigned long ms_hi  = ms_time / HI;


        // Use the cut off timer, with a medium cutoff if we haven't yet
        //  established a target plymax
        if( new_game || plymax_target == 0 )
        {
            plymax_target = 0;
            timer_set( ms_med );
        }

        // Else the cut off timer is more of an emergency brake, and normally
        //  we just re-run Sargon until we hit plymax_target
        else
        {
            timer_set( ms_hi );
        }
    }
    int plymax = 1;     // V1.00 Set plymax=3 as a baseline, it's more or less instant
                        // V1.01 Start from plymax=1 to support ultra bullet for example
    if( fixed_depth )
    {
        plymax = depth;  // It's more efficient but less informative (no progress reports)
                         //  to go straight to the final depth without iterating
    }
    int stalemates = 0;
    std::string bestmove_terse;
    unsigned long base = elapsed_milliseconds();
    bool repetition_avoid = false;
    PV repetition_fallback_pv;
    for(;;)
    {
        bool aborted = run_sargon(plymax,false);
        unsigned long now = elapsed_milliseconds();
        unsigned long elapsed = (now-base);
        if( aborted  || the_pv.variation.size()==0 )
        {
            log( "aborted=%s, pv.variation.size()=%d, elapsed=%lu, ms_lo=%lu, plymax=%d, plymax_target=%d\n",
                    aborted?"true @@":"false", the_pv.variation.size(), //@@ marks move in log
                    elapsed, ms_lo, plymax, plymax_target );
        }

        // Report on each normally concluded iteration
        if( !aborted && !repetition_avoid )
            generate_progress_report();

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

            // If attempts to avoid repetition leave us worse, fall back
            if( repetition_avoid && !aborted )
            {
                bool fallback=false;
                int score_after_repetition_avoid = the_pv.value;
                if( the_position.white )
                    fallback = (score_after_repetition_avoid <= 0);
                else
                    fallback = (score_after_repetition_avoid >= 0);
                if( fallback )
                    the_pv = repetition_fallback_pv;
                else
                    generate_progress_report();
                log( "After repetition avoidance, new value=%d, so %s\n", score_after_repetition_avoid, fallback ? "did fallback" : "didn't fallback" );
            }

            // Best move found
            bestmove_terse = the_pv.variation[0].TerseOut();
            std::string bestm = sargon_export_move(BESTM);
            if( !aborted && bestmove_terse.substr(0,4) != bestm )
                log( "Unexpected event: BESTM=%s != PV[0]=%s\n%s", bestm.c_str(), bestmove_terse.c_str(), the_position.ToDebugStr().c_str() );

            // Repetition avoidance always ends looping
            if( repetition_avoid )
            {
                plymax++;   // repetition avoidance run used decremented plymax, restore it
                break;
            }

            // If we have a move, and it checkmates opponent, don't iterate further!
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

            // If we timed out, target plymax should be reduced
            if( aborted )
            {
                plymax--;
                break;
            }

            // Otherwise keep iterating or not
            bool keep_going = false;
            if( fixed_depth )
            {
                // If fixed_depth, iterate until required depth
                if( plymax < depth )
                    keep_going = true;  // haven't reached required depth
            }
            else
            {
                // If not fixed_depth, according to the time management algorithm
                if( plymax_target<=0 || plymax<plymax_target )
                    keep_going = true;  // no target or haven't reached target
                else if( plymax_target>0 && plymax>=plymax_target && elapsed<ms_lo )
                    keep_going = true;  // reached target very quickly, so extend target
                else if( stalemates == 1 )
                    keep_going = true;  // try one more ply if we stalemate opponent!
                log( "elapsed=%lu, ms_lo=%lu, plymax=%d, plymax_target=%d, keep_going=%s\n", elapsed, ms_lo, plymax, plymax_target, keep_going?"true":"false @@" );  // @@ marks move in log
            }

            // If we are about to play move, and we're better; consider whether to kick in repetition avoidance
            if( !keep_going && (the_position.white? the_pv.value>0 : the_pv.value<0) )
            {
                thc::Move mv = the_pv.variation[0];
                if( test_whether_move_repeats(the_position,mv) )
                {
                    log( "Repetition avoidance, %s repeats\n", bestmove_terse.c_str() );
                    repetition_calculate( the_position, the_repetition_moves );
                    repetition_avoid = true;
                    repetition_fallback_pv = the_pv;
                    keep_going = true;
                    timer_clear();
                }
            }

            // Either keep going or end now
            if( !keep_going )
                break;  // end now
            else
            {
                if( !fixed_depth )
                {
                    if( !repetition_avoid )
                        plymax++;   // normal iteration
                    else
                    {
                        // have already used a full quota of time, so use less for the recalc
                        if( plymax > 0 )
                            plymax--; 
                    }
                }
            }
        }
    }
    if( !fixed_depth )
    {
        timer_clear();
        plymax_target = plymax;
    }
    thc::Move bestmove;
    bestmove.Invalid();
    bool have_move = bestmove.TerseIn( &the_position, bestmove_terse.c_str() );
    if( !have_move )
        log( "Sargon doesn't find move - %s\n%s", bestmove_terse.c_str(), the_position.ToDebugStr().c_str() );
    return bestmove;
}
#endif

// Simple logging facility gives us some debug capability when running under control of a GUI
static int log( const char *fmt, ... )
{
    static std::mutex mtx;
    std::lock_guard<std::mutex> lck(mtx);
	va_list args;
	va_start( args, fmt );
    if( logfile_name != "" )
    {
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
    }
    va_end(args);
    return 0;
}

// Calculate a list of moves that cause the position to repeat
//  Returns bool ok. If not ok, all moves repeat
static bool repetition_calculate( thc::ChessRules &cr, std::vector<thc::Move> &repetition_moves )
{
    bool ok=false;
    repetition_moves.clear();
    std::vector<thc::Move> v;
    cr.GenLegalMoveList(v);
    for( thc::Move mv: v )
    {
        if( test_whether_move_repeats(cr,mv) )
            repetition_moves.push_back(mv);
        else
            ok = true;
    }
    return ok;
}

static bool test_whether_move_repeats( thc::ChessRules &cr, thc::Move mv )
{
    // Unfortunately we can't PushMove() / PopMove() because PushMove()
    //  doesn't update the history buffer and the counts. PlayMove() does
    //  but we have to take care to restore the history buffer and the
    //  counts so that the change is temporary.
    // The need for this complication is a bit of a flaw in our thc
    //  library unfortunately and even needed a change; history_idx from
    //  protected to public.
    int restore_history = cr.history_idx;
    int restore_full    = cr.full_move_count;
    int restore_half    = cr.half_move_clock;
    cr.PlayMove(mv);
    int repetition_count = cr.GetRepetitionCount();
    cr.PopMove(mv);
    cr.history_idx      = restore_history;
    cr.full_move_count  = restore_full;
    cr.half_move_clock  = restore_half;
    return (repetition_count > 1);
}

struct NativeMove
{
    unsigned char ptr_lo;
    unsigned char ptr_hi;
    unsigned char square_src;
    unsigned char square_dst;
    unsigned char flags;
    unsigned char value;
};

static void show()
{
    unsigned char nply = peekb(NPLY);
    thc::ChessPosition cp;
    sargon_export_position(cp);
    std::string s = cp.ToDebugStr();
    printf( "%s\n", s.c_str() );
    printf( "NPLY = %02x\n", nply );
    unsigned int addr = PLYIX;
    unsigned int ptr  = peekw(addr);
    printf( "Ply ptrs;\n" );
    for( int i=0; i<4; i++ )
    {
        printf( "%04x", ptr );
        addr += 2;
        ptr  = peekw(addr);
        printf( i+1<4 ? " " : "\n" );
    }
    printf( "MLPTRI=%04x\n", peekw(MLPTRI) );
    printf( "MLPTRJ=%04x\n", peekw(MLPTRJ) );
    printf( "MLLST=%04x\n",  peekw(MLLST) );
    printf( "MLNXT=%04x\n",  peekw(MLNXT) );
    addr = 0x400;
    unsigned int mlnxt = peekw(MLNXT);
    for( int i=0; i<256; i++ )
    {
        ptr  = peekw(addr);
        printf( "%04x: %04x ", addr, ptr );
        printf( "%02x %02x %02x %02x ", peekb(addr+2), peekb(addr+3), peekb(addr+4), peekb(addr+5) );
        printf( "%s\n", sargon_export_move(addr,false).c_str() );
        if( addr == mlnxt )
            break;
        addr += 6;
    }
}



// Returns book okay
static bool repetition_test()
{
    // Test function repetition_calculate()
    //  After 1. Nf3 Nf6 2. Ng1 the move 2... Nf6-g8 repeats the initial position
    thc::ChessRules cr;
    thc::Move mv;
    mv.TerseIn(&cr,"g1f3");
    cr.PlayMove(mv);
    mv.TerseIn(&cr,"g8f6");
    cr.PlayMove(mv);
    mv.TerseIn(&cr,"f3g1");
    cr.PlayMove(mv);
    std::vector<thc::Move> w;
    repetition_calculate(cr,w);
    bool ok = true;
    if( w.size() != 1 )
        ok = false;
    else
    {
        if( w[0].src != thc::f6 )
            ok = false;
        if( w[0].dst != thc::g8 )
            ok = false;
    }

    // Non destructive test of function  repetition_remove_moves()
    unsigned int plyix = peekw(PLYIX);
    unsigned int mllst = peekw(MLLST);
    unsigned int mlnxt = peekw(MLNXT);
    const unsigned char *q = peek(0x400);
    unsigned char buf[18];
    memcpy(buf,q,18);

    // Calculate some Sargon squares (no sargon_import_square() unfortunately)
    unsigned int f3, e5, e1, f1, g1, h1;
    for( unsigned int j=0; j<256; j++ )
    {
        thc::Square sq;
        if( sargon_export_square(j,sq) )
        {
            if( sq == thc::f3 )
                f3 = j;
            else if( sq == thc::e5 )
                e5 = j;
            else if( sq == thc::e1 )
                e1 = j;
            else if( sq == thc::f1 )
                f1 = j;
            else if( sq == thc::g1 )
                g1 = j;
            else if( sq == thc::h1 )
                h1 = j;
        }
    }

    // Write 2 candidate moves into Sargon, with ptrs; First move is f3e5
    pokew(PLYIX,0x400);
    unsigned char *p = poke(0x400);
    *p++ = 6;
    *p++ = 4;
    *p++ = f3;
    *p++ = e5;
    *p++ = 0;
    *p++ = 0;
    pokew(MLLST,0x406);

    // O-O
    *p++ = 0;
    *p++ = 0;
    *p++ = e1;
    *p++ = g1;
    *p++ = 0x40;
    *p++ = 0;

    // O-O second move
    *p++ = 0;
    *p++ = 0;
    *p++ = h1;
    *p++ = f1;
    *p++ = 0;
    *p++ = 0;
    pokew(MLNXT,0x412);
    //show();

    // Remove f3e5
    std::vector<thc::Move> v;
    mv.src = thc::f3;
    mv.dst = thc::e5;
    v.push_back(mv);
    repetition_remove_moves(v);
    //show();

    // Check whether it matches our expectations
    if( 0x400 != peekw(MLLST) )
        ok = false;
    if( 0x40c != peekw(MLNXT) )
        ok = false;
    q = peek(0x400);
    if( *q++ != 0 )
        ok = false;
    if( *q++ != 0 )
        ok = false;
    if( *q++ != e1 )
        ok = false;
    if( *q++ != g1 )
        ok = false;
    if( *q++ != 0x40 )
        ok = false;
    if( *q++ != 0 )
        ok = false;
    if( *q++ != 0 )
        ok = false;
    if( *q++ != 0 )
        ok = false;
    if( *q++ != h1 )
        ok = false;
    if( *q++ != f1 )
        ok = false;
    if( *q++ != 0 )
        ok = false;
    if( *q++ != 0 )
        ok = false;

    // Undo all changes
    p = poke(0x400);
    memcpy(p,buf,18);
    pokew(PLYIX,plyix);
    pokew(MLLST,mllst);
    pokew(MLNXT,mlnxt);
    return ok;
}

// Remove candidate moves that will cause the position to repeat
static void repetition_remove_moves(  const std::vector<thc::Move> &repetition_moves  )
{
    //show();

    // Locate the list of candidate moves (ptr ends up being 0x400 always)
    unsigned int addr = PLYIX;
    unsigned int base = peekw(addr);
    unsigned int ptr  = base;

    // Read a vector of NativeMove
    unsigned int mlnxt = peekw(MLNXT);
    if( ptr!=0x400 || mlnxt<=ptr || ((mlnxt-ptr)%6)!=0 || ((mlnxt-ptr)/6>250) )
        return; // sanity checks
    std::vector<NativeMove> vin;
    while( ptr < mlnxt )
    {
        NativeMove nm;
        nm.ptr_lo = peekb(ptr++);
        nm.ptr_hi = peekb(ptr++);
        nm.square_src = peekb(ptr++);
        nm.square_dst = peekb(ptr++);
        nm.flags = peekb(ptr++);
        nm.value = peekb(ptr++);
        vin.push_back(nm);
    }

    // Create an edited (reduced) vector
    std::vector<NativeMove> vout;
    bool second_byte=false;
    bool copy_move_and_second_byte_if_present = true;
    for( NativeMove nm: vin )
    {
        if( second_byte )
            second_byte = false;
        else
        {
            if( nm.flags & 0x40 )
                second_byte = true;
            thc::Square src, dst;
            copy_move_and_second_byte_if_present = true;
            if( sargon_export_square(nm.square_src,src) && sargon_export_square(nm.square_dst,dst) )
            {
                for( thc::Move mv: repetition_moves )
                {
                    if( mv.src==src && mv.dst==dst )
                    {
                        copy_move_and_second_byte_if_present = false;
                        break;
                    }
                }
            }
        }
        if( copy_move_and_second_byte_if_present )
            vout.push_back(nm);
    }

    // Fixup ptr fields
    ptr = base;
    unsigned int ptr_final_move = ptr;
    unsigned int ptr_end = ptr + 6*vout.size();
    second_byte=false;
    for( NativeMove &nm: vout )
    {
        if( second_byte )
        {
            second_byte = false;
            nm.ptr_lo = 0;
            nm.ptr_hi = 0;
        }
        else
        {
            if( nm.flags & 0x40 )
                second_byte = true;
            ptr_final_move = ptr;
            unsigned int ptr_next = (second_byte ? ptr+12 : ptr+6);
            if( ptr_next == ptr_end )
                ptr_next = 0;
            nm.ptr_lo = ((ptr_next)&0xff);
            nm.ptr_hi = (((ptr_next)>>8)&0xff);
        }
        ptr += 6;
    }

    // Write vector back
    if( vout.size() )  // but if no moves left, make no changes
    {
        pokew( MLLST, ptr_final_move );
        pokew( MLNXT, ptr_end );
        ptr = base;
        for( NativeMove nm: vout )
        {
            pokeb( ptr++, nm.ptr_lo );
            pokeb( ptr++, nm.ptr_hi );
            pokeb( ptr++, nm.square_src );
            pokeb( ptr++, nm.square_dst );
            pokeb( ptr++, nm.flags );
            pokeb( ptr++, nm.value );
        }
    }

    //show();
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
        if( 0 == strcmp(msg,"after GENMOV()") )
        {
            genmov_callbacks++;
            if( peekb(NPLY)==1 && the_repetition_moves.size()>0 )
                repetition_remove_moves( the_repetition_moves );
        }
        else if( 0 == strcmp(msg,"end of POINTS()") )
        {
            end_of_points_callbacks++;
            sargon_pv_callback_end_of_points();
        }
        else if( 0 == strcmp(msg,"Yes! Best move") )
        {
            bestmove_callbacks++;
            sargon_pv_callback_yes_best_move();
        }

        // Abort run_sargon() if new event in queue (and not PLYMAX==1 which is
        //  effectively instantaneous, finds a baseline move)
        if( !async_queue.empty() && peekb(PLYMAX)>1 )
        {
            longjmp( jmp_buf_env, 1 );
        }
    }
};

