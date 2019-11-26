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

unsigned char *sargon_format_position_create( const char *fen );
std::string sargon_format_position_read( thc::ChessPosition &cp, const char *msg=0 );
void show_board_layout( const unsigned char *sargon_board, std::string msg );
void peek();
void diagnostics();
void pokeb( int offset, unsigned char b );
unsigned char peekb(int offset);
void sargon_tests();

// For peeking
static unsigned char *sargon_mem_base;
const int BOARD_SIZE    = 120;

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

extern "C" {
    void callback( uint32_t parameters )
    {
        uint32_t *sp = &parameters;
        //printf( "edi on stack at %p = 0x%08x\n", &sp[0], sp[0] );
        //printf( "esi on stack at %p = 0x%08x\n", &sp[1], sp[1] );
        //printf( "ebp on stack at %p = 0x%08x\n", &sp[2], sp[2] );
        //printf( "esp on stack at %p = 0x%08x\n", &sp[3], sp[3] );
        //printf( "ebx on stack at %p = 0x%08x\n", &sp[4], sp[4] );
        //printf( "edx on stack at %p = 0x%08x\n", &sp[5], sp[5] );
        //printf( "ecx on stack at %p = 0x%08x\n", &sp[6], sp[6] );
        //printf( "eax on stack at %p = 0x%08x\n", &sp[7], sp[7] );
        sp--;
        uint32_t ret_addr = *sp;
        //printf( "ret addr = 0x%08x\n", ret_addr );
        const unsigned char *code = (const unsigned char *)ret_addr;
        //printf( "jmp code = 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
        //    code[0], code[1], code[2], code[3], code[4], code[5], code[6], code[7] );

        // expecting 0xeb = 2 byte opcode, (0xeb + 8 bit relative jump),
        //  if others skip over 4 operand bytes
        const char *msg = ( code[0]==0xeb ? (char *)(code+2) : (char *)(code+6) );
        count_functions(msg);
        //printf( "Sargon diagnostics callback %s\n", msg );
        //if( msg[0] == '*' )
        //    peek();
        //else
        //    diagnostics();
    }
}

int main( int argc, const char *argv[] )
{
    util::tests();
    sargon_tests();
    on_exit_diagnostics();
}

void sargon_tests()
{
/*

Test position 2

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

Immediate capture of bishop b5xa6 (wins a piece) [1 ply]
Royal fork Nf5-e7 (wins queen) [3 ply]
Back rank mate Qd2xd8+ [4 ply maybe]

To make the back rank mate a little harder, move the
Pb5 and ba6 to Pb3 and ba4 so bishop can postpone mate
by one move.

ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff
ff 00 00 00 0c 00 00 0e 00 ff
ff 00 00 00 0d 00 01 01 01 ff
ff 00 00 00 00 00 00 00 00 ff
ff 00 00 00 00 00 00 00 00 ff
ff 00 09 00 00 00 0a 00 00 ff
ff 8b 00 00 00 00 00 8d 00 ff
ff 00 00 00 00 00 81 81 81 ff
ff 8c 00 00 8a 00 00 8e 00 ff
ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff


*/

    // CTWBFK = "Chess Tactics Workbook For Kids'
    const char *pos1 = "r2n2k1/5ppp/b5q1/1P3N2/8/8/3Q1PPP/3R2K1 w - - 0 1";             // Test position #1 above
    const char *pos1a = "r2n2k1/5pp1/b5qp/1P3N2/8/8/3Q1PPP/3R2K1 w - - 0 1";            // Slight mod, h7-h6 so it's not mate, yet Sargon still plays Qd2xNd8 with ply max = 1
                                                                                        //  and an easy piece capture on a6 available. Why?
    const char *pos1b = "r2n2k1/5pp1/b5qp/1P6/8/5N2/3Q1PPP/3R2K1 w - - 0 1";            //  Still does it even if we make N safe on f3 - at max ply 1 and 2
    const char *pos1c = "r2n4/5ppk/b5qp/1P6/8/5N2/3Q1PPP/3R2K1 w - - 0 1";              //  Even with king on h7 (so no check), still does it at max ply 1 (but not 2)
    const char *pos1d = "r2n4/5ppk/b5qp/8/1P6/5N2/3Q1PPP/2R3K1 w - - 0 1";              //  With king on h7 (so no check), pawn back to b4 (so no capture in reserve)
                                                                                        //   and rook on c1 (so giving up Q for N - not Q for N+R) Sargon plays Qxd8
                                                                                        //   at max ply 1 - so maybe it just thinks it is winning N and doesn't account
                                                                                        //   for defending rook at all.
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
    unsigned char *test_position = sargon_format_position_create( pos6 );

    sargon_mem_base = &sargon_base_address;
    unsigned char *sargon_board = sargon_mem_base + BOARDA;
    const unsigned char *sargon_move_made = sargon_mem_base + MVEMSG;
    pokeb(COLOR,0);
    pokeb(KOLOR,0);
    pokeb(PLYMAX,7);
    sargon(api_INITBD);
    thc::ChessPosition cp;
    std::string s = sargon_format_position_read( cp, "Position after board initialised" );
    printf( "%s\n", s.c_str() );
    memcpy( sargon_board, test_position, BOARD_SIZE );
    sargon(api_ROYALT);
    s = sargon_format_position_read( cp, "Position after test position set" );
    printf( "%s\n", s.c_str() );
    pokeb(MOVENO,3);    // Move number is 1 at at start, add 2 to avoid book move
    sargon(api_CPTRMV);
    s = sargon_format_position_read( cp, "Position after computer move made" );
    printf( "%s", s.c_str() );
    const unsigned char *q = sargon_move_made;
    printf( "\nMove made is: %c%c-%c%c\n", q[0],q[1],q[2],q[3] );
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
}

unsigned char *sargon_format_position_create( const char *fen )
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

    thc::ChessPosition cr;
    cr.Forsyth( fen );
    memset( board_position, 0xff, sizeof(board_position) );
    unsigned char *dst_base = board_position + 28;  // square h1
    char *src_base = &cr.squares[thc::h1];
    for( int i=0; i<8; i++ )
    {
        unsigned char *dst = dst_base + i*10;
        char *src = src_base - i*8;
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
                if( j==0 && c=='R' && cr.wking_allowed() )
                    moved = false; 
                if( (j==1||j==6) && c=='N' )
                    moved = false; 
                if( (j==2||j==5) && c=='B' )
                    moved = false; 
                if( j==3 && c=='K' && (cr.wking_allowed()||cr.wqueen_allowed()) )
                    moved = false; 
                if( j==4 && c=='Q' )
                    moved = false; 
                if( j==7 && c=='R' && cr.wqueen_allowed() )
                    moved = false; 
            }
            else if( i==1 && c=='P' )
                moved = false; 
            else if( i==6 && c=='p' )
                moved = false; 
            else if( i==7 )
            {
                if( j==0 && c=='r' && cr.bking_allowed() )
                    moved = false; 
                if( (j==1||j==6) && c=='n' )
                    moved = false; 
                if( (j==2||j==5) && c=='b' )
                    moved = false; 
                if( j==3 && c=='k' && (cr.bking_allowed()||cr.bqueen_allowed()) )
                    moved = false; 
                if( j==4 && c=='q' )
                    moved = false; 
                if( j==7 && c=='r' && cr.bqueen_allowed() )
                    moved = false; 
            }
            if( moved && b!=0 )
                b += 8;
            *dst-- = b;
        }
    }
    return board_position;
}

std::string sargon_format_position_read( thc::ChessPosition &cp, const char *msg )
{
    cp.Init();
    const unsigned char *sargon_board = sargon_mem_base + BOARDA;
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
    cp.white = (*(sargon_mem_base+COLOR) == 0x00);
    std::string s = cp.ToDebugStr(msg?msg:"");
    return s;
}

void show_board_layout( const unsigned char *sargon_board, std::string msg )
{
    printf( "%s\n", msg.c_str() );
    const unsigned char *p = sargon_board;
    for( int i=0; i<12; i++ )
    {
        for( int j=0; j<10; j++ )
            printf( "%02x%c", *p++, j+1<10?' ':'\n' );
    }
}

unsigned int peekw(int offset)
{
    const unsigned char *addr = sargon_mem_base + offset;
    const unsigned char lo = *addr++;
    const unsigned char hi = *addr++;
    unsigned int ret = hi;
    ret = ret <<8;
    ret += lo;
    return ret;
}

unsigned char peekb(int offset)
{
    const unsigned char *addr = sargon_mem_base + offset;
    unsigned int ret = *addr;
    return ret;
}

void pokeb( int offset, unsigned char b )
{
    unsigned char *addr = sargon_mem_base + offset;
    *addr = b;
}

void peek()
{
    printf( "MLPTRI = %04x\n", peekw(MLPTRI) );
    printf( "MLPTRJ = %04x\n", peekw(MLPTRJ) );
    printf( "MLLST  = %04x\n", peekw(MLLST) );
    printf( "MLNXT  = %04x\n", peekw(MLNXT) );
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

void diagnostics()
{
    peek();
    unsigned int p = peekw(MLPTRI);
    printf( "MLPTRI chain\n" );
    for( int i=0; i<50 && (p); i++ )
    {
        unsigned char from  = peekb(p+2);
        unsigned char to    = peekb(p+3);
        unsigned char flags = peekb(p+4);
        unsigned char value = peekb(p+5);
        p = peekw(p);
        printf( "link=0x%04x, from=%s, to=%s flags=0x%02x value=%u\n", p, algebraic(from).c_str(), algebraic(to).c_str(), flags, value );
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

