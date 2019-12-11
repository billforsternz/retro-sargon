/*

  A program to exercise the classic program Sargon, as presented in the book 
  "Sargon a Z80 Computer Chess Program" by Dan and Kathe Spracklen (Hayden Books
  1978). Another program in this suite converts the Z80 code to working
  X86 assembly language.
  
  */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
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
#include "sargon-asm-interface.h"
#include "translate.h"

// Read chess position from Sargon
void sargon_position_export( thc::ChessPosition &cp );

// Write chess position into Sargon
void sargon_position_import( const thc::ChessPosition &cp );

// Sargon square convention -> string
std::string algebraic( unsigned int sq );

// Peek and poke at Sargon
const unsigned char *peek(int offset);
unsigned char peekb(int offset);
unsigned int peekw(int offset);
unsigned char *poke(int offset);
void pokeb( int offset, unsigned char b );

// Misc diagnostics
void dbg_ptrs();
void diagnostics();
void sargon_tests();
void sargon_game();

std::map<std::string,unsigned long> func_counts;
void count_functions( const char *msg )
{
    std::string s(msg);
    auto it = func_counts.find(s);
    if( it == func_counts.end() )
        func_counts[s] = 1;
    else
        it->second++;
}

void on_exit_diagnostics()
{
    for( auto it = func_counts.begin(); it != func_counts.end(); ++it )
    {
        printf( "Callback %s called %lu times\n", it->first.c_str(), it->second );
    }
}
// Value seems to be 128 = 0.0
// 128-30 = 3.0
// 128+30 = -3.0
// 128-128 = 0 = -12.8
double sargon_value_export( unsigned int value )
{
    double m = -3.0 / 30.0;
    double c = 12.8;
    double y = m * value + c;
    return y;
}

unsigned int sargon_value_import( double value )
{
    if( value < -12.6 )
        value = -12.6;
    if( value > 12.6 )
        value = 12.6;
    double m = -10.0;
    double c = 128.0;
    double y = m * value + c;
    return static_cast<unsigned int>(y);
}

static std::map<std::string,std::string> positions;
static std::map<std::string,unsigned int> values;
static std::map<std::string,unsigned int> cardinal_nbr;

struct NODE
{
    unsigned char from;
    unsigned char to;
    unsigned char value;
    std::shared_ptr<NODE> child; 
    NODE( unsigned char f, unsigned char t, unsigned char v ) : from(f), to(t), value(v) {}
};

static std::vector< std::shared_ptr<NODE> > ply_table;

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

        // For purposes of minimax tracing experiment, we only want two possible
        //  moves in each position - achieved by suppressing King moves
        if( std::string(msg) == "Suppress King moves" )
        {
            unsigned char piece = peekb(T1);
            if( piece == 6 )    // King?
            {
                // Change al to 2 and ch to 1 and MPIECE will exit without
                //  generating (non-castling) king moves
                uint32_t *peax = &eax;
                *peax = 2;
                uint32_t *pecx = &ecx;
                *pecx = 0x100;
            }
        }

        // For purposes of minimax tracing experiment, we inject our own points
        //  score for each known position (we keep the number of positions to
        //  managable levels.)
        else if( std::string(msg) == "end of POINTS()" )
        {
            unsigned int p = peekw(MLPTRJ); // Load move list pointer
            unsigned char from  = peekb(p+2);
            unsigned char to    = peekb(p+3);
            unsigned int value = eax&0xff;
            thc::ChessPosition cp;
            sargon_position_export( cp );
            std::string sfrom = algebraic(from).c_str();
            std::string sto   = algebraic(to).c_str();

            std::string key = util::sprintf("%s%s -> %s", sfrom.c_str(), sto.c_str(), cp.squares );
            auto it = positions.find(key);
            if( it == positions.end() )
                printf( "key not found (?): %s\n", key.c_str() );
            else
            {
                std::string cardinal("??");
                auto it3 = cardinal_nbr.find(it->second);
                if( it3 != cardinal_nbr.end() )
                    cardinal = util::sprintf( "%d", it3->second );
                printf( "Position %s, %s found\n", cardinal.c_str(), it->second.c_str() );
                auto it2 = values.find(it->second);
                if( it2 == values.end() )
                    printf( "value not found (?): %s\n", it->second.c_str() );
                else
                {

                    // MODIFY VALUE !
                    value = it2->second;
                    uint32_t *peax = &eax;
                    *peax = value;
                }
            }
            bool was_white = (sfrom[0]=='a' || sfrom[0]=='b');
            cp.white = !was_white;
            static int count;
            std::string s = util::sprintf( "Position %d. Last move: from=%s, to=%s value=%d/%.1f", count++, algebraic(from).c_str(), algebraic(to).c_str(), value, sargon_value_export(value) );
            printf( "%s\n", cp.ToDebugStr(s.c_str()).c_str() );
        }

        // For purposes of minimax tracing experiment, try to figure out
        //  best move calculation
        else if( std::string(msg) == "Alpha beta cutoff?" )
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
#if 1
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
#endif
#if 0
            if( level < sizeof(pv)/sizeof(pv[0]) )
                pv[level] = n;
            if( level == 1 )
            {
                printf( "\nPV\n" );
                for( int i=1; i <= 5; i++ )
                {
                    NODE2 *n = &pv[i];
                    unsigned char from  = n->from;
                    unsigned char to    = n->to;
                    unsigned char flags = n->flags;
                    unsigned char value = n->value;
                    double fvalue = sargon_value_export(value);
                    printf( "from=%s, to=%s value=%d/%.1f, flags=%02x\n", algebraic(from).c_str(), algebraic(to).c_str(), value, fvalue, flags );
                }
            }
#endif
#if 0
            unsigned int p      = peekw(MLPTRJ);
            unsigned int level  = peekb(NPLY);
            unsigned char from  = peekb(p+2);
            unsigned char to    = peekb(p+3);
            unsigned char value = peekb(p+5);
            std::shared_ptr<NODE> ptr = std::make_shared<NODE>(from,to,value);
            static unsigned int previous_level;
            if( level > previous_level )
            {
                while( ply_table.size() < level )
                    ply_table.push_back(ptr);
            }
            else if( level == previous_level )
            {
                ply_table[ply_table.size()-1] = ptr;
            }
            else if( level < previous_level )
            {
                //if( level == 1 )
                //    printf("debug\n");
                while( ply_table.size() > level )
                {
                    std::shared_ptr<NODE> p2 = ply_table[ply_table.size()-1];
                    ply_table.pop_back();
                    if( ply_table.size() > 0 )
                    {
                        if( ply_table.size() == level )
                            ply_table[ply_table.size()-1] = ptr;
                        ply_table[ply_table.size()-1]->child = p2;
                    }
                }
            }
            previous_level = level;
#endif
        }
        else if( std::string(msg) == "After FNDMOV()" )
        {
            printf( "After FNDMOV()\n" );
            diagnostics();
        }
    }
};

void probe_test_prime( const thc::ChessPosition &cp )
{
    cardinal_nbr["root"]   = 0;
    cardinal_nbr["a4"]     = 1;
    cardinal_nbr["b4"]     = 2;
    cardinal_nbr["a4g5"]   = 3;
    cardinal_nbr["a4h5"]   = 4;
    cardinal_nbr["a4g5b4"] = 5;
    cardinal_nbr["a4g5a5"] = 6;
    cardinal_nbr["a4h5b4"] = 7;
    cardinal_nbr["a4h5a5"] = 8;
    cardinal_nbr["b4g5"]   = 9;
    cardinal_nbr["b4h5"]   = 10;
    cardinal_nbr["b4g5a4"] = 11;
    cardinal_nbr["b4g5b5"] = 12;
    cardinal_nbr["b4h5a4"] = 13;
    cardinal_nbr["b4h5b5"] = 14;

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

Best move found

MLPTRI = 0218
MLPTRJ = 0406
MLLST  = 041e
MLNXT  = 040c
PLYIX: 0400 0406 040c 0412 0418 041e 0 0 0 0 0 0 0 0 0 0 0 0 0 0
   MLPTRI: link=0x0406
         : link=0x0000, from=b3, to=b4 flags=0x00 value=68
   MLPTRJ: link=0x0000, from=b3, to=b4 flags=0x00 value=68
    MLLST: link=0x0000, from=b4, to=b5 flags=0x00 value=98
    MLNXT: link=0x0412, from=g6, to=g5 flags=0x00 value=68
         : link=0x0000, from=h6, to=h5 flags=0x00 value=88
 PLYIX[0]: link=0x0406, from=a3, to=a4 flags=0x00 value=68
         : link=0x0000, from=b3, to=b4 flags=0x00 value=68
 PLYIX[2]: link=0x0000, from=b3, to=b4 flags=0x00 value=68    *
 PLYIX[4]: link=0x0412, from=g6, to=g5 flags=0x00 value=68
         : link=0x0000, from=h6, to=h5 flags=0x00 value=88
 PLYIX[6]: link=0x0000, from=h6, to=h5 flags=0x00 value=88   *
 PLYIX[8]: link=0x041e, from=a3, to=a4 flags=0x00 value=118
         : link=0x0000, from=b4, to=b5 flags=0x00 value=98
PLYIX[10]: link=0x0000, from=b4, to=b5 flags=0x00 value=98  *

*/
    values["root"]   = sargon_value_import(0.0);
    values["a4"]     = sargon_value_import(8.0);
    values["a4g5"]   = sargon_value_import(7.0);
    values["a4g5b4"] = sargon_value_import(5.0);
    values["a4g5a5"] = sargon_value_import(4.0);
    values["a4h5"]   = sargon_value_import(3.0);
    values["a4h5b4"] = sargon_value_import(1.0);
    values["a4h5a5"] = sargon_value_import(2.0);
    values["b4"]     = sargon_value_import(6.0);
    values["b4g5"]   = sargon_value_import(5.5);
    values["b4g5a4"] = sargon_value_import(4.0);
    values["b4g5b5"] = sargon_value_import(3.0);
    values["b4h5"]   = sargon_value_import(4.0);
    values["b4h5a4"] = sargon_value_import(3.0);
    values["b4h5b5"] = sargon_value_import(1.0);

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
                                           
                                           
Best move found
MLPTRI = 0218
MLPTRJ = 0400
MLLST  = 041e
MLNXT  = 040c
PLYIX: 0400 0400 040c 0412 0418 0418 0 0 0 0 0 0 0 0 0 0 0 0 0 0
   MLPTRI: link=0x0400
         : link=0x0406, from=a3, to=a4 flags=0x00 value=68
         : link=0x0000, from=b3, to=b4 flags=0x00 value=68
   MLPTRJ: link=0x0406, from=a3, to=a4 flags=0x00 value=68
         : link=0x0000, from=b3, to=b4 flags=0x00 value=68
    MLLST: link=0x0000, from=a4, to=a5 flags=0x00 value=0
    MLNXT: link=0x0412, from=g6, to=g5 flags=0x00 value=68
         : link=0x0000, from=h6, to=h5 flags=0x00 value=88
 PLYIX[0]: link=0x0406, from=a3, to=a4 flags=0x00 value=68
         : link=0x0000, from=b3, to=b4 flags=0x00 value=68
 PLYIX[2]: link=0x0406, from=a3, to=a4 flags=0x00 value=68
         : link=0x0000, from=b3, to=b4 flags=0x00 value=68
 PLYIX[4]: link=0x0412, from=g6, to=g5 flags=0x00 value=68
         : link=0x0000, from=h6, to=h5 flags=0x00 value=88
 PLYIX[6]: link=0x0000, from=h6, to=h5 flags=0x00 value=88
 PLYIX[8]: link=0x041e, from=b3, to=b4 flags=0x00 value=18
         : link=0x0000, from=a4, to=a5 flags=0x00 value=0
PLYIX[10]: link=0x041e, from=b3, to=b4 flags=0x00 value=18
         : link=0x0000, from=a4, to=a5 flags=0x00 value=0

*/

#if 1  // wake up to trace alpha-beta
    values["root"]   = sargon_value_import(0.0);
    values["a4"]     = sargon_value_import(6.0);
    values["a4g5"]   = sargon_value_import(6.0);
    values["a4g5b4"] = sargon_value_import(5.0);
    values["a4g5a5"] = sargon_value_import(4.0);
    values["a4h5"]   = sargon_value_import(4.0);
    values["a4h5b4"] = sargon_value_import(11.0);
    values["a4h5a5"] = sargon_value_import(2.0);
    values["b4"]     = sargon_value_import(6.0);
    values["b4g5"]   = sargon_value_import(6.0);
    values["b4g5a4"] = sargon_value_import(4.0);
    values["b4g5b5"] = sargon_value_import(3.0);
    values["b4h5"]   = sargon_value_import(4.0);
    values["b4h5a4"] = sargon_value_import(1.0);
    values["b4h5b5"] = sargon_value_import(3.0);
#endif    
    thc::Move mv;
    thc::ChessPosition base = cp;
    thc::ChessRules work = base;
    std::string pos = std::string(work.squares);
    const char *txt="0 ??0 ??";
    std::string key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "root";

    //  1.a4 g5 2.a5
    work = base;
    mv.TerseIn( &work, txt="a3a4" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "a4";
    mv.TerseIn( &work, txt="g6g5" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "a4g5";
    mv.TerseIn( &work, txt="a4a5" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "a4g5a5";

    //  1.a4 g5 2.b4
    work = base;
    mv.TerseIn( &work, txt="a3a4" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="g6g5" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="b3b4" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "a4g5b4";

    //  1.a4 h5 2.a5 
    work = base;
    mv.TerseIn( &work, txt="a3a4" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="h6h5" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "a4h5";
    mv.TerseIn( &work, txt="a4a5" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "a4h5a5";

    //  1.a4 h5 2.b4
    work = base;
    mv.TerseIn( &work, txt="a3a4" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="h6h5" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="b3b4" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "a4h5b4";

    //  1.b4 h5 2.b5 
    work = base;
    mv.TerseIn( &work, txt="b3b4" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "b4";
    mv.TerseIn( &work, txt="h6h5" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "b4h5";
    mv.TerseIn( &work, txt="b4b5" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "b4h5b5";

    //  1.b4 h5 2.a4
    work = base;
    mv.TerseIn( &work, txt="b3b4" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="h6h5" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="a3a4" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "b4h5a4";

    //  1.b4 g5 2.b5
    work = base;
    mv.TerseIn( &work, txt="b3b4" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="g6g5" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "b4g5";
    mv.TerseIn( &work, txt="b4b5" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "b4g5b5";

    //  1.b4 g5 2.a4
    work = base;
    mv.TerseIn( &work, txt="b3b4" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="g6g5" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="a3a4" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "b4g5a4";

    for( auto it = positions.begin(); it != positions.end(); ++it )
    {
        printf( "%s: %s\n", it->second.c_str(), it->first.c_str() );
    }
}

int main( int argc, const char *argv[] )
{
    util::tests();
    sargon_tests();
    //sargon_game();
    on_exit_diagnostics();
}


void sargon_game()
{
    std::ofstream f("moves.txt");
    if( !f )
    {
        printf( "Error; Cannot open moves.txt\n" );
        return;
    }
    thc::ChessRules cr;
    for(;;)
    {
        char buf[5];
        pokeb(MVEMSG,   0 );
        pokeb(MVEMSG+1, 0 );
        pokeb(MVEMSG+2, 0 );
        pokeb(MVEMSG+3, 0 );
        pokeb(COLOR,0);
        pokeb(KOLOR,0);
        pokeb(PLYMAX,5);
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
        bool ok = mv.TerseIn( &cr, buf );
        if( !ok || trigger )
        {
            printf( "%s - %s\n%s", ok?"Castling or En-Passant":"Sargon doesnt find move", buf, cr.ToDebugStr().c_str() );
            printf( "(After)\n%s",cr_after.ToDebugStr().c_str() );
            if( !ok )
                break;
        }
        std::string s = mv.NaturalOut(&cr);
        printf( "Sargon plays %s\n", s.c_str() );
        util::putline( f, s );
        cr.PlayMove(mv);
        if( strcmp(cr.squares,cr_after.squares) != 0 )
        {
            printf( "Position mismatch\n" );
            //break;
        }
        std::vector<thc::Move> moves;
        thc::ChessEvaluation ce=cr;
        ce.GenLegalMoveListSorted( moves );
        if( moves.size() == 0 )
        {
            printf( "Tarrasch doesnt find move\n" );
            break;
        }
        mv = moves[0];
        s = mv.NaturalOut(&cr);
        printf( "Tarrasch plays %s\n", s.c_str() );
        util::putline( f, s );
        cr.PlayMove(mv);
    }
}

void sargon_tests()
{
/*

Test position 1

FEN r2n2k1/5ppp/b5q1/1P3N2/8/8/3Q1PPP/3R2K1 w - - 0 1

r..n..k.
.....ppp
b.....q.
.P...N..
........
........
...Q.PPP
...R..K.

White to play has three forcing wins, requiring
increasing depth for increasing reward;

In theory:
Immediate capture of bishop b5xa6 (wins a piece) [1 ply]
Royal fork Nf5-e7 (wins queen) [3 ply]
Back rank mate Qd2xd8+ [4 ply maybe]
In practice:
Mate found with PLYMAX 3 or greater
Royal fork found with PLYMAX 1 or 2

To make the back rank mate a little harder, move the
Pb5 and ba6 to Pb3 and ba4 so bishop can postpone mate
by one move. As expected mate now requires greater
depth, so royal fork for PLYMAX 1-4, mate if PLYMAX 5

*/

    // CTWBFK = "Chess Tactics Workbook For Kids'
    const char *pos1 = "r2n2k1/5ppp/b5q1/1P3N2/8/8/3Q1PPP/3R2K1 w - - 0 1";             // Test position #1 above
    const char *pos1b = "r2n2k1/5ppp/6q1/5N2/b7/1P6/3Q1PPP/3R2K1 w - - 0 1";            // Modified version as discussed above
                                                                                        // Used to have a lot of other mods as we
                                                                                        // tried to figure out what was going wrong before
                                                                                        // we got some things sorted out (notably need to
                                                                                        // call ROYALT before CPTRMV!)
    const char *pos2 = "2r1nrk1/5pbp/1p2p1p1/8/p2B4/PqNR2P1/1P3P1P/1Q1R2K1 w - - 0 1";  // CTWBFK Pos 30, page 41 - solution Nc3-d5
    const char *pos3 = "5k2/3KR3/4B3/8/3P4/8/8/6q1 w - - 0 1";                          // CTWBFK Pos 34, page 62 - solution Re7-f7+
    const char *pos4 = "3r2k1/1pq2ppp/pb1pp1b1/8/3B4/2N5/PPP1QPPP/4R1K1 w - - 0 1";     // CTWBFK Pos 7, page 102 - solution Nc3-d5
                                                                                        //  Sargon currently fails on this one - Plays Bd4xb6 instead
                                                                                        //  a -2 move instead of a +2 move, with reasonable depth
                                                                                        //  Now fixed! after adding call to ROYALT() after setting position
    const char *pos5 = "r4r2/6kp/2pqppp1/p1R5/b2P4/4QN2/1P3PPP/2R3K1 w - - 0 1";        // CTWBFK Pos 29, page 77 - solution Qe3-a3. Quite difficult!
    const char *pos6 = "2rq1r1k/3npp1p/3p1n1Q/pp1P2N1/8/2P4P/1P4P1/R4R1K w - - 0 1";    // CTWBFK Pos 11, page 68 - solution Rf1xf6. Sadly Sargon doesn't solve this one
                                                                                        //  It now does! At PLYMAX=7, takes about 5 mins 45 secs
    const char *pos7 = "8/8/q2pk3/2p5/8/3N4/8/4K2R w K - 0 1";      // White has Nc3xd5+ pulling victory from the jaws of defeat 
                                                                    //  It's seen at PLYMAX=3, not seen at PLYMAX=2
                                                                    //  It's a kind of 5 ply calculation (on the 5th half move White captures the queen
                                                                    //  which is the only justification for the sac on the 1st half move) - so maybe
                                                                    //  add 2 to convert PLYMAX to calculation depth
    const char *pos8 = "3k4/8/8/7P/8/8/1p6/1K6 w - - 0 1";  // Pawn outside the square needs PLYMAX=5 to solve
    const char *pos9 = "2k5/8/8/8/7P/8/1p6/1K6 w - - 0 1";  // Pawn one further step back needs, as expected PLYMAX=7 to solve
    const char *pos_probe = "7k/8/6pp/8/8/PP6/8/K7 w - - 0 1";      // W king on a1 pawns a3 and b3, B king on h8 pawns g6 and h6 we are going
                                                                    //  to use this very dumb position to probe Alpha Beta pruning etc. (we
                                                                    //  will kill the kings so that each side has only two moves available
                                                                    //  at each position)
    thc::ChessPosition cp;
    cp.Forsyth(pos5);
    probe_test_prime(cp);
    pokeb(COLOR,0);
    pokeb(KOLOR,0);
    pokeb(PLYMAX,5);
    sargon(api_INITBD);
    sargon_position_import(cp);
    sargon(api_ROYALT);
    sargon_position_export(cp);
    std::string s = cp.ToDebugStr( "Position after test position set" );
    printf( "%s\n", s.c_str() );
    pokeb(MOVENO,3);    // Move number is 1 at at start, add 2 to avoid book move
    sargon(api_CPTRMV);
    sargon_position_export(cp);
    s = cp.ToDebugStr( "Position after computer move made" );
    printf( "%s\n", s.c_str() );
    char buf[5];
    memcpy( buf, peek(MVEMSG), 4 );
    buf[4] = '\0';
    printf( "\nMove made is: %s\n", buf );
    int offset = MLEND;
    while( offset > 0 )
    {
        unsigned char b = peekb(offset);
        if( b )
        {
            printf("Last non-zero in memory is addr=0x%04x data=0x%02x\n", offset, b );
            break;
        }
        offset--;
    }

    // Print move chain
#if 1
    int nbr = nodes.size();
    int search_start = nbr-1;
    unsigned int target = 1;
    int last_found=0;
    for(;;)
    {
        for( int i=search_start; i>=0; i-- )
        {
            NODE2 *n = &nodes[i];
            unsigned int level = n->level;
            if( level == target )
            {
                target++;
                last_found = i;
                unsigned char from  = n->from;
                unsigned char to    = n->to;
                unsigned char value = n->value;
                double fvalue = sargon_value_export(value);
                printf( "level=%d, from=%s, to=%s value=%d/%.1f\n", n->level, algebraic(from).c_str(), algebraic(to).c_str(), value, fvalue );
            }
        }
        if( target == 1 )
            break;
        else
        {
            printf("\n");
            target = 1;
            search_start = last_found-1;
        }
    }
    for( int i=0; i<nbr; i++ )
    {
        NODE2 *n = &nodes[i];
        unsigned int level = n->level;
        unsigned char from  = n->from;
        unsigned char to    = n->to;
        unsigned char value = n->value;
        double fvalue = sargon_value_export(value);
        printf( "level=%d, from=%s, to=%s value=%d/%.1f\n", n->level, algebraic(from).c_str(), algebraic(to).c_str(), value, fvalue );
    }
#endif
#if 0
    std::shared_ptr<NODE> solution = ply_table.size()==0 ? NULL : ply_table[0];
    while( solution )
    {
        unsigned char from  = solution->from;
        unsigned char to    = solution->to;
        unsigned char value = solution->value;
        double fvalue = sargon_value_export(value);
        printf( "from=%s, to=%s value=%d/%.1f\n", algebraic(from).c_str(), algebraic(to).c_str(), value, fvalue );
        solution = solution->child;
    }
#endif
    diagnostics();
}

// Read chess position from Sargon
void sargon_position_export( thc::ChessPosition &cp )
{
    cp.Init();
    const unsigned char *sargon_board = peek(BOARDA);
    const unsigned char *src_base = sargon_board + 28;  // square h1
    char *dst_base = &cp.squares[thc::h1];
    for( int i=0; i<8; i++ )
    {
        const unsigned char *src = src_base + i*10;
        char *dst = dst_base - i*8;
        for( int j=0; j<8; j++ )
        {
            unsigned char b = *src--;
            char c = ' ';
            b &= 0x87;
            switch( b )
            {
                case 0x81:   c = 'p';   break;
                case 0x82:   c = 'n';   break;
                case 0x83:   c = 'b';   break;
                case 0x84:   c = 'r';   break;
                case 0x85:   c = 'q';   break;
                case 0x86:   c = 'k';   break;
                case 0x01:   c = 'P';   break;
                case 0x02:   c = 'N';   break;
                case 0x03:   c = 'B';   break;
                case 0x04:   c = 'R';   break;
                case 0x05:   c = 'Q';   break;
                case 0x06:   c = 'K';   break;
            }
            *dst-- = c;
        }
    }
    cp.white = (peekb(COLOR) == 0x00);
}

// Write chess position into Sargon
void sargon_position_import( const thc::ChessPosition &cp )
{
    static unsigned char board_position[120] =
    {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0x04, 0x02, 0x03, 0x05, 0x06, 0x03, 0x02, 0x04, 0xff,  // <-- White back row, a1 at left end, h1 at right end
        0xff, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xff,  // <-- White pawns
        0xff, 0xa3, 0xb3, 0xc3, 0xd3, 0xe3, 0xf3, 0x00, 0x00, 0xff,  // <-- note 'a3' etc just documents square convention
        0xff, 0xa4, 0xb4, 0xc4, 0xd4, 0xe4, 0xf4, 0x00, 0x00, 0xff,  // <-- but g and h files are 0x00 = empty
        0xff, 0xa5, 0xb5, 0xc5, 0xd5, 0xe5, 0xf5, 0x00, 0x00, 0xff,
        0xff, 0xa6, 0xb6, 0xc6, 0xd6, 0xe6, 0xf6, 0x00, 0x00, 0xff,
        0xff, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xff,  // <-- Black pawns
        0xff, 0x84, 0x82, 0x83, 0x85, 0x86, 0x83, 0x82, 0x84, 0xff,  // <-- Black back row
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };

    memset( board_position, 0xff, sizeof(board_position) );
    unsigned char *dst_base = board_position + 28;  // square h1
    const char *src_base = &cp.squares[thc::h1];
    for( int i=0; i<8; i++ )
    {
        unsigned char *dst = dst_base + i*10;
        const char *src = src_base - i*8;
        for( int j=0; j<8; j++ )
        {
            char c = *src--;
            unsigned char b = 0;
            switch( c )
            {
                case 'p':   b = 0x81;   break;
                case 'n':   b = 0x82;   break;
                case 'b':   b = 0x83;   break;
                case 'r':   b = 0x84;   break;
                case 'q':   b = 0x85;   break;
                case 'k':   b = 0x86;   break;
                case 'P':   b = 0x01;   break;
                case 'N':   b = 0x02;   break;
                case 'B':   b = 0x03;   break;
                case 'R':   b = 0x04;   break;
                case 'Q':   b = 0x05;   break;
                case 'K':   b = 0x06;   break;
            }
            bool moved=true;
            if( i==0 )
            {
                if( j==0 && c=='R' && cp.wking_allowed() )
                    moved = false; 
                if( (j==1||j==6) && c=='N' )
                    moved = false; 
                if( (j==2||j==5) && c=='B' )
                    moved = false; 
                if( j==3 && c=='K' && (cp.wking_allowed()||cp.wqueen_allowed()) )
                    moved = false; 
                if( j==4 && c=='Q' )
                    moved = false; 
                if( j==7 && c=='R' && cp.wqueen_allowed() )
                    moved = false; 
            }
            else if( i==1 && c=='P' )
                moved = false; 
            else if( i==6 && c=='p' )
                moved = false; 
            else if( i==7 )
            {
                if( j==0 && c=='r' && cp.bking_allowed() )
                    moved = false; 
                if( (j==1||j==6) && c=='n' )
                    moved = false; 
                if( (j==2||j==5) && c=='b' )
                    moved = false; 
                if( j==3 && c=='k' && (cp.bking_allowed()||cp.bqueen_allowed()) )
                    moved = false; 
                if( j==4 && c=='q' )
                    moved = false; 
                if( j==7 && c=='r' && cp.bqueen_allowed() )
                    moved = false; 
            }
            if( moved && b!=0 )
                b += 8;
            *dst-- = b;
        }
    }
    memcpy( poke(BOARDA), board_position, sizeof(board_position) );
}

const unsigned char *peek(int offset)
{
    unsigned char *sargon_mem_base = &sargon_base_address;
    unsigned char *addr = sargon_mem_base + offset;
    return addr;
}

unsigned char peekb(int offset)
{
    const unsigned char *addr = peek(offset);
    return *addr;
}

unsigned int peekw(int offset)
{
    const unsigned char *addr = peek(offset);
    const unsigned char lo = *addr++;
    const unsigned char hi = *addr++;
    unsigned int ret = hi;
    ret = ret <<8;
    ret += lo;
    return ret;
}

unsigned char *poke(int offset)
{
    unsigned char *sargon_mem_base = &sargon_base_address;
    unsigned char *addr = sargon_mem_base + offset;
    return addr;
}

void pokeb( int offset, unsigned char b )
{
    unsigned char *addr = poke(offset);
    *addr = b;
}

std::string algebraic( unsigned int sq )
{
    std::string s = util::sprintf( "%d ??",sq);
    if( sq >= 21 )
    {
        int rank = (sq-21)/10;     // eg a1=21 =0, h1=28=0, a2=31=1
        int file = sq - (21 + (rank*10));
        if( 0<=rank && rank<=7 && 0<=file && file<=7 )
        {
            s = util::sprintf( "%c%c", 'a'+file, '1'+rank );
        }
    }
    return s;
}

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


/*
 *  Sadly we need this junk to get thc.h and thc.cpp to compile and link
 */

#ifndef KILL_DEBUG_COMPLETELY
int core_printf( const char *fmt, ... )
{
    int ret=0;
	va_list args;
	va_start( args, fmt );
    char buf[1000];
    char *p = buf;
    vsnprintf( p, sizeof(buf)-2-(p-buf), fmt, args ); 
    fputs(buf,stdout);
    va_end(args);
    return ret;
}
#endif

void ReportOnProgress
(
    bool    init,
    int     multipv,
    std::vector<thc::Move> &pv,
    int     score_cp,
    int     depth
)
{
}

