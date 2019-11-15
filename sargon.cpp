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
#include "sargon-constants.h"
#include "translate.h"

static void convert( std::string fin, std::string asm_fout,  std::string report_fout, std::string h_constants_fout );

unsigned char *sargon_format_position_create( const char *fen );
std::string sargon_format_position_read( thc::ChessPosition &cp, const char *msg=0 );
void show_board_layout( const unsigned char *sargon_board, std::string msg );
void peek();
void diagnostics();

struct shim_parameters
{
    uint32_t command;
    unsigned char *board;
};

extern "C" {
    int shim_function( shim_parameters *parm );
    void test_inspect();
}

// For peeking
static const unsigned char *sargon_mem_base;
const int BOARD_SIZE    = 120;

/*
const int BOARD_OFFSET  = 0x0134;
#if 0
const int MLPTRI = 0x021b;
const int MLPTRJ = 0x021d;
const int MLLST  = 0x0223;
const int MLNXT  = 0x0225;
const int COLOR  = 0x0228;
const int MVEMSG = 0x0247;
#else
const int MLPTRI = 0x021f;
const int MLPTRJ = 0x0221;
const int MLLST  = 0x0227;
const int MLNXT  = 0x0229;
const int COLOR  = 0x022c;
const int MVEMSG = 0x024b;
#endif
*/

extern "C" {
    void inspector( const char *msg )
    {
        printf( "Sargon diagnostics callback %s\n", msg );
        if( msg[0] == '*' )
            peek();
        else
            diagnostics();
    }
}

int main( int argc, const char *argv[] )
{
    util::tests();

#if 0
    test_inspect();
#endif

#if 0
    convert("sargon-step6.asm","output-step6.asm", "report-step6.txt", "sargon-constants.h" );
#endif

#if 0
    bool ok = (argc==4);
    if( !ok )
    {
        printf(
            "Read, understand, convert sargon source code\n"
            "Usage:\n"
            " convert sargon.asm sargon-out.asm report.txt\n"
        );
        return -1;
    }
    convert(argv[1],argv[2],argv[3]);
#endif

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

#if 1
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
    unsigned char *test_position = sargon_format_position_create( pos5 );

    //convert("sargon-step6.asm","output-step6.asm", "report-step6.txt");
    shim_parameters sh;
    sh.command = 0;
    shim_function( &sh );
    unsigned char *sargon_board = sh.board;
    sargon_mem_base = sargon_board - BOARDA;
    const unsigned char *sargon_move_made = sargon_mem_base + MVEMSG;
    thc::ChessPosition cp;
    std::string s = sargon_format_position_read( cp, "Position after board initialised" );
    printf( "%s\n", s.c_str() );
    memcpy( sargon_board, test_position, BOARD_SIZE );
    sh.command = 2;
    shim_function( &sh );   // Adjust king and queen positions
    s = sargon_format_position_read( cp, "Position after test position set" );
    printf( "%s\n", s.c_str() );
    sh.command = 1;
    shim_function( &sh );
    s = sargon_format_position_read( cp, "Position after computer move made" );
    printf( "%s", s.c_str() );
    const unsigned char *q = sargon_move_made;
    printf( "\nMove made is: %c%c-%c%c\n", q[0],q[1],q[2],q[3] );
#endif
    return 0;
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


enum statement_typ {empty, discard, illegal, comment_only, comment_only_indented, directive, equate, normal};

struct statement
{
    statement_typ typ;
    std::string label;
    std::string equate;
    std::string directive;
    std::string instruction;
    std::vector<std::string> parameters;
    std::string comment;
};

struct name_plus_parameters
{
    //name_plus_parameters( std::set<std::vector<std::string>> &p ) : name(n), parameters(p) {}
    std::string name;
    std::set<std::vector<std::string>> parameters;
};

static void convert( std::string fin, std::string asm_fout , std::string report_fout, std::string h_constants_fout )
{
    std::ifstream in(fin);
    if( !in )
    {
        printf( "Error; Cannot open file %s for reading\n", fin.c_str() );
        return;
    }
    std::ofstream out(report_fout);
    if( !out )
    {
        printf( "Error; Cannot open file %s for writing\n", report_fout.c_str() );
        return;
    }
    std::ofstream asm_out(asm_fout);
    if( !asm_out )
    {
        printf( "Error; Cannot open file %s for writing\n", asm_fout.c_str() );
        return;
    }
    std::ofstream h_out(h_constants_fout);
    if( !h_out )
    {
        printf( "Error; Cannot open file %s for writing\n", h_constants_fout.c_str() );
        return;
    }
    std::set<std::string> labels;
    std::map< std::string, std::vector<std::string> > equates;
    std::map< std::string, std::set<std::vector<std::string>> > instructions;
    bool data_mode = true;
    translate_init();

    // Each source line can optionally be transformed to Z80 mnemonics (or hybrid Z80 plus X86 registers mnemonics)
    enum { transform_none, transform_z80, transform_hybrid } transform_switch = transform_none;

    // After optional transformation, the original line can be kept, discarded or commented out
    enum { original_keep, original_comment_out, original_discard } original_switch = original_discard;

    // Generated equivalent code can optionally be generated, in various flavours
    enum { generate_x86, generate_z80, generate_hybrid, generate_none } generate_switch = generate_x86;

    // .IF controls let us switch between three modes (currently)
    enum { mode_normal, mode_pass_thru, mode_suspended } mode = mode_normal;

    unsigned int track_location = 0;
    for(;;)
    {
        std::string line;
        if( !std::getline(in,line) )
            break;
        std::string line_original = line;
        util::replace_all(line,"\t"," ");
        statement stmt;
        stmt.typ = normal;
        stmt.label = "";
        stmt.equate = "";
        stmt.instruction = "";
        stmt.parameters.clear();
        stmt.comment = "";
        util::rtrim(line);
        bool done = false;

        // Discards
        if( line.length() == 0 )
        {
            stmt.typ = empty;
            done = true;
        }
        else if( line[0] == '<' )
        {
            stmt.typ = discard;
            done = true;
        }

        // Get rid of comments
        if( !done )
        {
            size_t offset = line.find(';');
            if( offset != std::string::npos )
            {
                stmt.comment = line.substr(offset+1);
                line = line.substr(0,offset);
                util::rtrim(line);
                if( offset==0 || line.length()==0 )
                {
                    stmt.typ = offset==0 ? comment_only : comment_only_indented;
                    done = true;
                }
            }
        }

        // Labels and directives
        if( !done && line[0]!=' ' )
        {
            stmt.typ = line[0]=='.' ? directive : equate;
            if( stmt.typ == directive )
                stmt.directive = line;
            for( unsigned int i=0; i<line.length(); i++ )
            {
                char c = line[i];
                if( isascii(c) && !isalnum(c) ) 
                {
                    if( c==':' && i>0 )
                    {
                        stmt.typ = normal;
                        stmt.label = line.substr(0,i);
                        std::string temp = line.substr(i+1);
                        line = " " + temp;
                        break;
                    }
                    else if( c==' ' && stmt.typ == directive )
                    {
                        stmt.directive = line.substr(0,i);
                        line = line.substr(i);
                        break;
                    }
                    else if( c==' ' && stmt.typ == equate )
                    {
                        stmt.equate = line.substr(0,i);
                        line = line.substr(i);
                        break;
                    }
                }
            }
            if( stmt.typ==equate && stmt.equate=="" )
                stmt.typ = illegal;
            if( stmt.typ == illegal )
                done = true;
        }

        // Get statement and parameters
        if( !done && line[0]==' ' )
        {
            util::ltrim(line);
            line += " ";    // to get last parameter
            bool in_parm = true;
            int start = 0;
            for( unsigned int i=0; i<line.length(); i++ )
            {
                char c = line[i];
                if( in_parm )
                {
                    if( c==' ' || c==',' )
                    {
                        std::string parm = line.substr( start, i-start );
                        in_parm = false;
                        if( parm.length() > 0 )
                        {
                            if( start==0 && stmt.typ==equate )
                            {
                                if( parm != "=" )
                                {
                                    stmt.typ = illegal;
                                    break;
                                }
                            }
                            else if( start==0 && stmt.typ==normal )
                                stmt.instruction = parm;
                            else
                                stmt.parameters.push_back(parm);
                        }
                    }
                }
                else
                {
                    if( c!=' ' && c!=',' )
                    {
                        start = i;
                        in_parm = true;
                    }
                }
            }
        }

        // Reduce to a few simple types of line
        std::string line_out="";
        done = false;
        switch(stmt.typ)
        {
            case empty:
                line_out = "EMPTY"; done=true;
                break;
            case discard:
                line_out = "DISCARD"; done=true; break;
            case illegal:
                line_out = "ILLEGAL> ";
                line_out += line_original;
                done = true;
                break;
            case comment_only:
            case comment_only_indented:
                line_out = stmt.typ==comment_only_indented ? "COMMENT_ONLY> " : "COMMENT_ONLY_INDENTED> ";
                line_out += stmt.comment;
                done = true;
                break;
            case directive:             // looks like "directives" are simply unindented normal instructions
                stmt.typ = normal;
                stmt.instruction = stmt.directive;  // and fall through
            case normal:
                line_out = "NORMAL";
                if( stmt.instruction == "BYTE" )
                    stmt.instruction = ".BYTE";
                if( stmt.instruction == "WORD" )
                    stmt.instruction = ".WORD";
                break;
            case equate:
                line_out = "EQUATE"; break;
        }

        // Line by line reporting
        if( !done && mode==mode_normal )
        {
            if( stmt.label != "" )
            {
                labels.insert(stmt.label);
                line_out += " label: ";
                line_out += "\"";
                line_out += stmt.label;
                line_out += "\"";
            }
            if( stmt.equate != "" )
            {
                line_out += " equate: ";
                line_out += "\"";
                line_out += stmt.equate;
                line_out += "\"";
                auto it = equates.find(stmt.equate);
                if( it != equates.end() )
                    line_out += " Error: dup equate";
                else
                    equates.insert( std::pair<std::string,std::vector<std::string>> (stmt.equate, stmt.parameters) );
            }
            if( stmt.instruction != "" )
            {
                line_out += " instruction: ";
                line_out += "\"";
                line_out += stmt.instruction;
                line_out += "\"";
                instructions[stmt.instruction].insert(stmt.parameters);
            }
            bool first=true;
            for( std::string parm: stmt.parameters )
            {
                line_out += first ? " parameters: " : ",";
                line_out += "\"";
                line_out += parm;
                line_out += "\"";
                first = false;
            }
        }
        if( !done && mode==mode_normal )
            util::putline(out,line_out);

        // My invented directives
        bool handled=false;
        if( stmt.label=="" )
        {
            if( stmt.instruction == ".DATA" )
            {
                data_mode = true;
                handled = true;         
            }
            else if( stmt.instruction == ".CODE" )
            {
                data_mode = false;
                handled = true;         
            }
            else if( stmt.instruction == ".IF_Z80" )
            {
                mode = mode_suspended;
                handled = true;         
            }
            else if( stmt.instruction == ".IF_16BIT" )
            {
                mode = mode_normal;
                handled = true;         
            }
            else if( stmt.instruction == ".IF_X86" )
            {
                mode = mode_pass_thru;
                handled = true;         
            }
            else if( stmt.instruction == ".ELSE" )
            {
                if( mode == mode_suspended )
                    mode = mode_pass_thru;
                else if( mode == mode_pass_thru )
                    mode = mode_suspended;
                else if( mode == mode_normal )
                    mode = mode_suspended;
                else
                    printf( "Error, unexpected .ELSE\n" );
                handled = true;         
            }
            else if( stmt.instruction == ".ENDIF" )
            {
                if( mode == mode_suspended ||  mode == mode_pass_thru )
                    mode = mode_normal;
                else
                    printf( "Error, unexpected .ENDIF\n" );
                handled = true;         
            }
        }
        if( mode==mode_pass_thru && !handled )
        {
            util::putline( asm_out, line_original );
            continue;
        }

        // Generate assembly language output
        switch( stmt.typ )
        {
            case empty:
            case comment_only:
            case comment_only_indented:
                line_original = detabify(line_original);
                util::putline( asm_out, line_original );
                break;
        }
        if( stmt.typ!=normal && stmt.typ!=equate )
            continue;

        if( handled || mode == mode_suspended  || mode == mode_pass_thru )
            continue;

        // Optionally transform source lines to Z80 mnemonics
        if( transform_switch!=transform_none )
        {
            if( stmt.equate=="" && stmt.instruction!="")
            {
                std::string out;
                bool transformed = translate_z80( line_original, stmt.instruction, stmt.parameters, transform_switch==transform_hybrid, out );
                if( transformed )
                {
                    line_original = stmt.label;
                    if( stmt.label == "" )
                        line_original = "\t";
                    else
                        line_original += (data_mode?"\t":":\t");
                    line_original += out;
                    if( stmt.comment != "" )
                    {
                        line_original += "\t;";
                        line_original += stmt.comment;
                    }
                }
            }
        }
        switch( original_switch )
        {
            case original_comment_out:
            {
                util::putline( asm_out, detabify( ";" + line_original) );
                break;
            }
            case original_keep:
            {
                util::putline( asm_out, detabify(line_original) );
                break;
            }
            default:
            case original_discard:
                break;
        }

        // Optionally generate code
        if( generate_switch != generate_none )
        {
            std::string str_location = (generate_switch==generate_z80 ? "$" : util::sprintf( "0%xh", track_location ) );
            std::string asm_line_out;
            if( stmt.equate != "" )
            {
                asm_line_out = stmt.equate;
                asm_line_out += "\tEQU";
                bool first = true;
                for( std::string s: stmt.parameters )
                {
                    if( first && s[0]=='.' )
                        s = str_location + s.substr(1);
                    asm_line_out += first ? "\t" : ",";
                    first = false;
                    asm_line_out += s;
                }
                if( first )
                {
                    printf( "Error: No EQU parameters, line=[%s]\n", line.c_str() );
                }
            }
            else if( stmt.label != "" && stmt.instruction=="" )
            {
                if( data_mode )
                {
                    asm_line_out = util::sprintf( "%s\tEQU\t%s", stmt.label.c_str(), str_location.c_str() );
                    std::string c_include_line_out = util::sprintf( "const int %s = 0x%04x;", stmt.label.c_str(), track_location  );
                    util::putline( h_out, c_include_line_out );
                }
                else
                    asm_line_out = stmt.label + ":";
                if( stmt.comment != "" )
                {
                    asm_line_out += "\t;";
                    asm_line_out += stmt.comment;
                }
            }
            else if( stmt.instruction!="" )
            {
                std::string out;
                bool generated = false;
                bool show_original = false;
                if( generate_switch == generate_z80 )
                {
                    generated = translate_z80( line_original, stmt.instruction, stmt.parameters, generate_switch==generate_hybrid, out );
                    show_original = !generated;
                }
                else
                {
                    if( data_mode && (stmt.instruction == ".LOC" || stmt.instruction == ".BLKB"  ||
                                      stmt.instruction == ".BYTE" || stmt.instruction == ".WORD")
                      )
                    {
                        if( stmt.parameters.size() == 0 )
                        {
                            printf( "Error, at least one parameter required. Line: [%s]\n", line_original.c_str() );
                            show_original = true;
                        }
                        else
                        {
                            bool first = true;
                            bool commented = false;
                            std::string parameter_list;
                            for( std::string s: stmt.parameters )
                            {
                                parameter_list += first ? "" : ",";
                                first = false;
                                parameter_list += s;
                            }
                            if( stmt.instruction == ".LOC" )
                            {
                                asm_line_out += util::sprintf( ";\tORG\t%s", parameter_list.c_str() );
                                std::string s=stmt.parameters[0];
                                unsigned int len = s.length();
                                unsigned int base = 10;
                                if( len>0 && (s[len-1]=='H' || s[len-1]=='h') )
                                {
                                    len--;
                                    s = s.substr(0,len);
                                    base = 16;
                                }
                                unsigned int accum=0;
                                bool err = (len<1);
                                for( char c: s )
                                {
                                    accum *= base;
                                    if( '0'<=c && c<='9' )
                                        accum += (c-'0');
                                    else if( base==16 && 'a'<=c && c<='f' )
                                        accum += (10+c-'a');
                                    else if( base==16 && 'A'<=c && c<='F' )
                                        accum += (10+c-'A');
                                    else
                                    {
                                        err = true;
                                        break;
                                    }
                                }
                                if( err )
                                {
                                    printf( "Error, .LOC parameter is unparseable. Line: [%s]\n", line_original.c_str() );
                                    show_original = true;
                                }
                                else if( accum < track_location )
                                {
                                    printf( "Error, .LOC parameter attempts unsupported reposition earlier in memory. Line: [%s]\n", line_original.c_str() );
                                    show_original = true;
                                }
                                else if( accum > track_location )
                                {
                                    asm_line_out += util::sprintf( "\n\tDB\t%d\tDUP (?)", accum - track_location );
                                }
                                track_location = accum;
                            }
                            if( stmt.label != "" )
                            {
                                std::string c_include_line_out = util::sprintf( "const int %s = 0x%04x;", stmt.label.c_str(), track_location  );
                                util::putline( h_out, c_include_line_out );
                                asm_line_out = util::sprintf( "%s\tEQU\t%s", stmt.label.c_str(), str_location.c_str() );
                                if( stmt.comment != "" )
                                {
                                    asm_line_out += "\t;";
                                    asm_line_out += stmt.comment;
                                    commented = true;
                                }
                                asm_line_out += "\n";
                            }
                            if( stmt.instruction == ".BLKB" )
                            {
                                asm_line_out += util::sprintf( "\tDB\t%s\tDUP (?)", parameter_list.c_str() );
                                unsigned int nbr = atoi( stmt.parameters[0].c_str() );
                                if( nbr == 0 )
                                {
                                    printf( "Error, .BLKB parameter is zero or unparseable. Line: [%s]\n", line_original.c_str() );
                                    show_original = true;
                                }
                                track_location += nbr;
                            }
                            else if( stmt.instruction == ".BYTE" )
                            {
                                asm_line_out += util::sprintf( "\tDB\t%s", parameter_list.c_str() );
                                track_location += stmt.parameters.size();
                            }
                            else if( stmt.instruction == ".WORD" )
                            {
                                asm_line_out += util::sprintf( "\tDW\t%s", parameter_list.c_str() );
                                track_location += (2 * stmt.parameters.size());
                            }
                            if( !commented && stmt.comment != "" )
                            {
                                asm_line_out += "\t;";
                                asm_line_out += stmt.comment;
                            }
                        }
                    }
                    else
                    {
                        generated = translate_x86( line_original, stmt.instruction, stmt.parameters, labels, out );
                        show_original = !generated;
                    }
                }
                if( show_original )
                    asm_line_out = line_original;
                if( generated )
                {
                    asm_line_out = stmt.label;
                    if( stmt.label == "" )
                        asm_line_out = "\t";
                    else
                        asm_line_out += (data_mode?"\t":":\t");
                    if( original_switch == original_comment_out )
                        asm_line_out += out;    // don't worry about comment
                    else
                    {
                        size_t offset = out.find('\n');
                        if( offset == std::string::npos )
                        {
                            asm_line_out += out;
                            if( stmt.comment != "" )
                            {
                                asm_line_out += "\t;";
                                asm_line_out += stmt.comment;
                            }
                        }
                        else
                        {
                            asm_line_out += out.substr(0,offset);
                            if( stmt.comment != "" )
                            {
                                asm_line_out += "\t;";
                                asm_line_out += stmt.comment;
                            }
                            asm_line_out += out.substr(offset);
                        }
                    }
                }
            }
            asm_line_out = detabify(asm_line_out);
            util::putline( asm_out, asm_line_out );
        }
    }

    // Summary report
    util::putline(out,"\nLABELS\n");
    for( const std::string &s: labels )
    {
        util::putline(out,s);
    }
    util::putline(out,"\nEQUATES\n");
    for( const std::pair<std::string,std::vector<std::string>> &p: equates )
    {
        std::string s;
        s += p.first;
        s += ": ";
        bool init = true;
        for( const std::string &t: p.second )
        {
            s += init?" ":", ";
            init = false;
            s += t;
        }
        util::putline(out,s);
    }
    util::putline(out,"\nINSTRUCTIONS\n");
    for( const std::pair<std::string,std::set<std::vector<std::string>> > &p: instructions )
    {
        std::string s;
        s += p.first;
        util::putline(out,s);
        for( const std::vector<std::string> &v: p.second )
        {
            s = " >";
            bool init = true;
            for( const std::string &t: v )
            {
                s += init?" ":", ";
                init = false;
                s += t;
            }
            util::putline(out,s);
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

