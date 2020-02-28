/*

  This program tests a Windows port of the classic program Sargon, as
  presented in the book "Sargon a Z80 Computer Chess Program" by Dan and
  Kathe Spracklen (Hayden Books 1978). Another program in this suite converts
  the Z80 code to working X86 assembly language. A third program wraps the
  Sargon X86 code in a simple standard Windows UCI engine interface.
  
  */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include "util.h"
#include "thc.h"
#include "sargon-asm-interface.h"
#include "sargon-interface.h"

// Individual tests
bool sargon_algorithm_explore( bool quiet, bool alpha_beta );

// Suites of tests
bool sargon_tests_quiet();

static std::string get_position_identifier();
static void new_test();

// Nodes to track the PV (principal [primary?] variation)
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
static std::vector< NODE > nodes;

// Control callback() behaviour
static bool callback_enabled;
static bool callback_kingmove_suppressed;
static int callback_verbosity;
static std::vector<unsigned int> cardinal_list;

// Prime the callback routine to modify node values for simple 15 node minimax
//  algorithm example
void probe_test_prime( bool alpha_beta );


int main( int argc, const char *argv[] )
{
    new_test();
    return 0;
}


// Data structures to track the 15 positions in minimax example
static std::map<std::string,unsigned int> values;
static std::map<std::string,unsigned int> cardinal_nbr;

// Prime the callback routine to modify node values for simple 15 node minimax
//  algorithm example
void probe_test_prime( bool alpha_beta )
{
    cardinal_nbr["(root)"]  = 0;
    cardinal_nbr["A"]       = 1;
    cardinal_nbr["B"]       = 2;
    cardinal_nbr["AG"]      = 3;
    cardinal_nbr["AH"]      = 4;
    cardinal_nbr["AGB"]     = 5;
    cardinal_nbr["AGA"]     = 6;
    cardinal_nbr["AHB"]     = 7;
    cardinal_nbr["AHA"]     = 8;
    cardinal_nbr["BG"]      = 9;
    cardinal_nbr["BH"]      = 10;
    cardinal_nbr["BGA"]     = 11;
    cardinal_nbr["BGB"]     = 12;
    cardinal_nbr["BHA"]     = 13;
    cardinal_nbr["BHB"]     = 14;

/*

Example 1, no Alpha Beta pruning (move played is 1.b4, value 3.0)

     <-MAX       <-MIN       <-MAX

0-------+-----1-----+-----3-----+------5 
0.0->3.0|   1.a4    |   1...g5  |    2.b4
        | 8.0->2.0  |  7.0->5.0 |       5.0
        |           |           |     <-5.0
        |           |           |     
        |           |           +------6 
        |           |                2.a5
        |           |                   4.0
        |           |               
        |           |               
        |           +-----4-----+------7 
        |              1...h5   |    2.b4
        |              3.0->2.0 |       1.0
        |                 <-2.0 |        
        |                       |     
        |                       +------8 
        |                            2.a5
        |                               2.0
        |                             <-2.0
        |                           
        +------2----+-----9-----+------11
            1.b4    |  1...g5   |    2.a4
          6.0->3.0  |  5.5->4.0 |       4.0
             <-3.0  |           |     <-4.0
                    |           |    
                    |           |    
                    |           +------12
                    |                2.b5
                    |                   3.0
                    |             
                    |             
                    |             
                    +-----10----+------13
                       1...h5   |    2.a4
                       4.0->3.0 |       3.0
                          <-3.0 |     <-3.0
                                |    
                                |    
                                +------14
                                     2.b5
                                        1.0
                                        1.0

*/
    if( !alpha_beta )
    {
        values["(root)"] = sargon_import_value(0.0);
        values["A"]      = sargon_import_value(8.0);
        values["AG"]     = sargon_import_value(7.0);
        values["AGB"]    = sargon_import_value(5.0);
        values["AGA"]    = sargon_import_value(4.0);
        values["AH"]     = sargon_import_value(3.0);
        values["AHB"]    = sargon_import_value(1.0);
        values["AHA"]    = sargon_import_value(2.0);
        values["B"]      = sargon_import_value(6.0);
        values["BG"]     = sargon_import_value(5.5);
        values["BGA"]    = sargon_import_value(4.0);
        values["BGB"]    = sargon_import_value(3.0);
        values["BH"]     = sargon_import_value(4.0);
        values["BHA"]    = sargon_import_value(3.0);
        values["BHB"]    = sargon_import_value(1.0);
    }
/*

Example 2, Alpha Beta pruning (move played is 1.a4, value 5.0)

     <-MAX       <-MIN       <-MAX

0-------+-----1-----+-----3-----+------5 
0.0->5.0|   1.a4    |   1...g5  |    2.b4
        | 6.0->5.0  |  6.0->5.0 |       5.0
        |    <-5.0  |     <-5.0 |     <-5.0
        |           |           |     
        |           |           +------6 
        |           |                2.a5
        |           |                   4.0
        |           |               
        |           |               
        |           +-----4-----+------7 
        |              1...h5   |    2.b4
        |              4.0      |      11.0  Cutoff 11.0>5.0, guarantees 4,7,8 won't affect score of
        |                       |            node 1 (nodes marked with * not evaluated)
        |                       |     
        |                       +------8*
        |                                  
        |                                  
        |                                  
        |                           
        +------2----+-----9-----+------11
            1.b4    |  1...g5   |    2.a4
          6.0->4.0  |  6.0->4.0 |       4.0
                    |     <-4.0 |     <-4.0
                    |           |    
                    |           |    
                    |           +------12
                    |                2.b5
                    |                   3.0  Cutoff, at this point we can be sure node 2 is going to be
                    |                        4.0 or lower, irrespective of nodes 10,13,14, therefore
                    |                        node 2 cannot beat node 1
                    |             
                    +-----10----+------13*
                       1...h5   |    2.a4
                       4.0      |          
                                |    
                                |    
                                |    
                                +------14*
                                     2.b5
                                           

*/

    if( alpha_beta )
    {
        values["(root)"] = sargon_import_value(0.0);
        values["A"]      = sargon_import_value(6.0);
        values["AG"]     = sargon_import_value(6.0);
        values["AGB"]    = sargon_import_value(5.0);
        values["AGA"]    = sargon_import_value(4.0);
        values["AH"]     = sargon_import_value(4.0);
        values["AHB"]    = sargon_import_value(11.0);
        values["AHA"]    = sargon_import_value(2.0);
        values["B"]      = sargon_import_value(6.0);
        values["BG"]     = sargon_import_value(6.0);
        values["BGA"]    = sargon_import_value(4.0);
        values["BGB"]    = sargon_import_value(3.0);
        values["BH"]     = sargon_import_value(4.0);
        values["BHA"]    = sargon_import_value(1.0);
        values["BHB"]    = sargon_import_value(3.0);
    }
}


// Simplified version
void probe_test_prime_simplified()
{
    cardinal_nbr["(root)"]  = 0;
    cardinal_nbr["A"]       = 1;
    cardinal_nbr["B"]       = 2;
    cardinal_nbr["AG"]      = 3;
    cardinal_nbr["AH"]      = 4;
    cardinal_nbr["AGA"]     = 5;
    cardinal_nbr["AGB"]     = 6;
    cardinal_nbr["AHA"]     = 7;
    cardinal_nbr["AHB"]     = 8;
    cardinal_nbr["BG"]      = 9;
    cardinal_nbr["BH"]      = 10;
    cardinal_nbr["BGA"]     = 11;
    cardinal_nbr["BGB"]     = 12;
    cardinal_nbr["BHA"]     = 13;
    cardinal_nbr["BHB"]     = 14;

/*

Example 1, no Alpha Beta pruning (move played is 1.b4, value 3.0)

     <-MAX       <-MIN       <-MAX

0-------+-----1-----+-----3-----+------5 
0.0->3.0|   1.a4    |   1...g5  |    2.b4
        | 8.0->2.0  |  7.0->5.0 |       5.0
        |           |           |     <-5.0
        |           |           |     
        |           |           +------6 
        |           |                2.a5
        |           |                   4.0
        |           |               
        |           |               
        |           +-----4-----+------7 
        |              1...h5   |    2.b4
        |              3.0->2.0 |       1.0
        |                 <-2.0 |        
        |                       |     
        |                       +------8 
        |                            2.a5
        |                               2.0
        |                             <-2.0
        |                           
        +------2----+-----9-----+------11
            1.b4    |  1...g5   |    2.a4
          6.0->3.0  |  5.5->4.0 |       4.0
             <-3.0  |           |     <-4.0
                    |           |    
                    |           |    
                    |           +------12
                    |                2.b5
                    |                   3.0
                    |             
                    |             
                    |             
                    +-----10----+------13
                       1...h5   |    2.a4
                       4.0->3.0 |       3.0
                          <-3.0 |     <-3.0
                                |    
                                |    
                                +------14
                                     2.b5
                                        1.0
                                        1.0

*/
    values["(root)"] = sargon_import_value(0.0); //(0.0);
    values["A"]      = sargon_import_value(0.0); //(8.0);
    values["AG"]     = sargon_import_value(0.0); //(7.0);
    values["AGB"]    = sargon_import_value(5.0);
    values["AGA"]    = sargon_import_value(4.0);
    values["AH"]     = sargon_import_value(0.0); //(3.0);
    values["AHB"]    = sargon_import_value(1.0);
    values["AHA"]    = sargon_import_value(2.0);
    values["B"]      = sargon_import_value(0.0); //(6.0);
    values["BG"]     = sargon_import_value(0.0); //(5.5);
    values["BGA"]    = sargon_import_value(2.1);  // <-- 2.1, 2.5, 3.0, 4.0 ok 0.0, 1.0, 1.9, 2.0 not ok MUST BE > "AHA"
    values["BGB"]    = sargon_import_value(0.0);
    values["BH"]     = sargon_import_value(0.0); //(4.0);
    values["BHA"]    = sargon_import_value(0.0);
    values["BHB"]    = sargon_import_value(0.0);
}

// Calculate a short string key to represent the moves played
//  eg 1. a4-a5 h6-h5 2. a5-a6 => "AHA"
static std::string get_position_identifier()
{
    thc::ChessPosition cp;
    sargon_export_position( cp );
    std::string key = "??";
    int nmoves = 0; // work out how many moves have been played
    bool a0 = (cp.squares[thc::a3] == 'P');
    bool a1 = (cp.squares[thc::a4] == 'P');
    bool a2 = (cp.squares[thc::a5] == 'P');
    if( a1 )
        nmoves += 1;
    else if( a2 )
        nmoves += 2;
    bool b0 = (cp.squares[thc::b4] == 'P');
    bool b1 = (cp.squares[thc::b5] == 'P');
    bool b2 = (cp.squares[thc::b6] == 'P');
    if( b1 )
        nmoves += 1;
    else if( b2 )
        nmoves += 2;
    bool g0 = (cp.squares[thc::g6] == 'p');
    bool g1 = (cp.squares[thc::g5] == 'p');
    if( g1 )
        nmoves += 1;
    bool h0 = (cp.squares[thc::h6] == 'p');
    bool h1 = (cp.squares[thc::h5] == 'p');
    if( h1 )
        nmoves += 1;
    if( nmoves == 0 )
    {
        key = "(root)";
    }
    else if( nmoves == 1 )
    {
        if( a1 )
            key = "A";
        else if( b1 )
            key = "B";
    }
    else if( nmoves == 2 )
    {
        if( a1 && g1 )
            key = "AG";
        else if( a1 && h1 )
            key = "AH";
        else if( b1 && g1  )
            key = "BG";
        else if( b1 && h1 )
            key = "BH";
    }
    else if( nmoves == 3 )
    {
        if( a2 && g1 )
            key = "AGA";
        else if( a2 && h1 )
            key = "AHA";
        else if( b2 && g1 )
            key = "BGB";
        else if( b2 && h1 )
            key = "BHB";
        else
        {
            // In other three move cases we can't work out the whole sequence of moves unless we know
            //  the last move, try to rely on this as little as possible
            bool a_last = false;
            bool b_last = false;
            bool g_last = false;
            bool h_last = false;
            unsigned int p = peekw(MLPTRJ);         // Load ptr to last move, use this to disambiguate if required
            unsigned char from  = p ? peekb(p+2) : 0;
            thc::Square sq_from;
            bool ok_from = sargon_export_square(from,sq_from);
            if( ok_from )
            {
                a_last = (thc::get_file(sq_from) == 'a');
                b_last = (thc::get_file(sq_from) == 'b');
                g_last = (thc::get_file(sq_from) == 'g');
                h_last = (thc::get_file(sq_from) == 'h');
            }
            if( a1 && g1 && b1 )
            {
                if ( a_last )
                    key = "BGA";
                else if ( b_last )
                    key = "AGB";
                else
                    key = "[A|B]G[B|A]";    // just in case, never actually observed
            }
            else if( a1 && h1 && b1 )
            {
                if ( a_last )
                    key = "BHA";
                else if ( b_last )
                    key = "AHB";
                else
                    key = "[A|B]H[B|A]";    // just in case, never actually observed
            }
        }
    }
    return key;
}

// Use a simple example to explore/probe the minimax algorithm and verify it
static void new_test()
{
    // White king on a1 pawns a4,b3 Black king on h8 pawns g6,h6 we are going
    //  to use this very dumb position to probe Alpha Beta pruning etc. (we
    //  will kill the kings so that each side has only two moves available
    //  at each position).
    // (Start 'a' pawn on a4 instead of a3 so that 'a' pawn move is generated
    // first on White's second move, even if 'b' pawn advances on first move)
    const char *pos_probe = "7k/8/6pp/8/1P6/P7/8/K7 w - - 0 1";

    // Because there are only 2 moves available at each ply, we can explore
    //  to PLYMAX=3 with only 2 positions at ply 1, 4 positions at ply 2
    //  and 8 positions at ply 3 (plus 1 root position at ply 0) for a very
    //  manageable 1+2+4+8 = 15 nodes (i.e. positions) total. We use the
    //  callback facility to monitor the algorithm and indeed actively
    //  interfere with it by changing the node evals and watching how that
    //  effects node traversal and generates a best move.
    // Note that if alpha_beta=true the resulting node values will result
    // in alpha-beta pruning and all 15 nodes won't be traversed
    //probe_test_prime(false);
    probe_test_prime_simplified();
    callback_enabled = true;
    callback_kingmove_suppressed = true;
    callback_verbosity = 0;
    cardinal_list.clear();
    thc::ChessPosition cp;
    cp.Forsyth(pos_probe);
    pokeb(MLPTRJ,0); //need to set this ptr to 0 to get Root position recognised in callback()
    pokeb(MLPTRJ+1,0);
    pokeb(KOLOR,0);
    pokeb(PLYMAX,3);
    sargon(api_INITBD);
    sargon_import_position(cp);
    sargon(api_ROYALT);
    pokeb(MOVENO,3);    // Move number is 1 at at start, add 2 to avoid book move
    nodes.clear();
    sargon(api_CPTRMV);
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

        if( 0 == strcmp(msg,"LDAR") )
        {
            // For testing purposes, make LDAR output increment, results in
            //  deterministic choice of book moves
            static uint8_t a_reg;
            a_reg++;
            volatile uint32_t *peax = &reg_eax;
            *peax = a_reg;
            return;
        }
        else if( 0 == strcmp(msg,"Yes! Best move") )
        {
            unsigned int  p     = peekw(MLPTRJ);
            unsigned int  level = peekb(NPLY);
            unsigned char from  = peekb(p+2);
            unsigned char to    = peekb(p+3);
            unsigned char flags = peekb(p+4);
            unsigned char value = peekb(p+5);
            NODE n(level,from,to,flags,value);
            nodes.push_back(n);
        }
        if( !callback_enabled )
            return;

        // For purposes of minimax tracing experiment, we only want two possible
        //  moves in each position - achieved by suppressing King moves
        if( callback_kingmove_suppressed && std::string(msg) == "Suppress King moves" )
        {
            unsigned char piece = peekb(T1);
            if( piece == 6 )    // King?
            {
                // Change al to 2 and ch to 1 and MPIECE will exit without
                //  generating (non-castling) king moves
                volatile uint32_t *peax = &reg_eax;
                *peax = 2;
                volatile uint32_t *pecx = &reg_ecx;
                *pecx = 0x100;
            }
        }

        // For purposes of minimax tracing experiment, we inject our own points
        //  score for each known position (we keep the number of positions to
        //  managable levels.)
        else if( callback_kingmove_suppressed && std::string(msg) == "end of POINTS()" )
        {
            std::string key = get_position_identifier();
            std::string cardinal("??");
            auto it = cardinal_nbr.find(key);
            if( it != cardinal_nbr.end() )
            {                                  
                cardinal_list.push_back(it->second);
                cardinal = util::sprintf( "%d", it->second );
            }
            printf( "Position %s, \"%s\" found\n", cardinal.c_str(), key.c_str() );
            auto it2 = values.find(key);
            if( it2 == values.end() )
                printf( "value not found (?): %s\n", key.c_str() );
            else
            {
                // MODIFY VALUE !
                unsigned int value = it2->second;
                volatile uint32_t *peax = &reg_eax;
                *peax = value;
            }
            if( callback_verbosity >= 3 )
            {
                unsigned int p = peekw(MLPTRJ); // Load move list pointer
                unsigned char from  = p ? peekb(p+2) : 0;
                unsigned char to    = p ? peekb(p+3) : 0;
                unsigned int value = reg_eax&0xff;
                thc::ChessPosition cp;
                sargon_export_position( cp );
                thc::Square sq_from, sq_to;
                bool ok = sargon_export_square(from,sq_from);
                bool root = !ok;
                sargon_export_square(to,sq_to);
                char f1 = thc::get_file(sq_from);
                char f2 = thc::get_rank(sq_from);
                char t1 = thc::get_file(sq_to);
                char t2 = thc::get_rank(sq_to);
                bool was_black = (root || f1=='g' || f1=='h');
                cp.white = was_black;
                static int count;
                std::string s;
                if( root )
                    s = util::sprintf( "%d> value=%d/%.1f", count++, value, sargon_export_value(value) );
                else
                    s = util::sprintf( "%d>. Last move: %c%c%c%c value=%d/%.1f", count++, f1,f2,t1,t2, value, sargon_export_value(value) );
                printf( "%s\n", cp.ToDebugStr(s.c_str()).c_str() );
            }
        }

        // For purposes of minimax tracing experiment, try to figure out
        //  best move calculation
        else if( callback_verbosity>0 && std::string(msg) == "Alpha beta cutoff?" )
        {
            printf( "Eval %d, %s\n", peekb(NPLY), get_position_identifier().c_str() );
            if( callback_verbosity > 1 )
            {
                unsigned int al  = reg_eax&0xff;
                unsigned int bx  = reg_ebx&0xffff;
                unsigned int val = peekb(bx);
                bool jmp = (al <= val);
                printf( "Ply level %d\n", peekb(NPLY));
                printf( "Alpha beta cutoff? Yes if move value=%d/%.1f <= two lower ply value=%d/%.1f, ",
                    al,  sargon_export_value(al),
                    val, sargon_export_value(val) );
                printf( "So %s\n", jmp?"yes":"no" );
            }
        }
        else if( callback_verbosity>1 && std::string(msg) == "No. Best move?" )
        {
            unsigned int al  = reg_eax&0xff;
            unsigned int bx  = reg_ebx&0xffff;
            unsigned int val = peekb(bx);
            bool jmp = (al <= val);
            printf( "Ply level %d\n", peekb(NPLY));
            printf( "Best move? No if move value=%d/%.1f <= one lower ply value=%d/%.1f, ",
                al,  sargon_export_value(al),
                val, sargon_export_value(val) );
            printf( "So %s\n", jmp?"no":"yes" );
        }
        else if( callback_verbosity>0 && std::string(msg) == "Yes! Best move" )
        {
            printf( "Best move, ply level %d, %s\n", peekb(NPLY), get_position_identifier().c_str() );
            if( callback_verbosity > 1 )
            {
                unsigned int p      = peekw(MLPTRJ);
                unsigned char from  = peekb(p+2);
                unsigned char to    = peekb(p+3);
                printf( "Best move found: %s%s\n", algebraic(from).c_str(), algebraic(to).c_str() );
            }
        }
    }
};

