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
#include "translate.h"

static void convert( std::string fin, std::string asm_fout,  std::string report_fout );
unsigned char *gen_sargon_format_position( const char *fen );

struct shim_parameters
{
    uint32_t command;
    unsigned char *board;
};

extern "C" {
    int shim_function( shim_parameters *parm );
}

int main( int argc, const char *argv[] )
{
    util::tests();

#if 0
    convert("sargon-step6.asm","output-step6.asm", "report-step6.txt");
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
    const char *pos2 = "2r1nrk1/5pbp/1p2p1p1/8/p2B4/PqNR2P1/1P3P1P/1Q1R2K1 w - - 0 1";  // CTWBFK Pos 30, page 41 - solution Nc3-d5
    unsigned char *test_position = gen_sargon_format_position( pos2 );

    //convert("sargon-step6.asm","output-step6.asm", "report-step6.txt");
    shim_parameters sh;
    sh.command = 0;
    shim_function( &sh );
    unsigned char *sargon_board = sh.board;
    unsigned char *sargon_move_made = sargon_board + (0x247-0x134);
    unsigned char board_position[120] =
    {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x0e, 0x00, 0xff,
        0xff, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x01, 0x01, 0x01, 0xff,
        0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
        0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
        0xff, 0x00, 0x09, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0xff,
        0xff, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8d, 0x01, 0xff,
        0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81, 0x81, 0x00, 0xff,
        0xff, 0x8c, 0x00, 0x00, 0x00, 0x8a, 0x00, 0x8e, 0x00, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };

    printf( "Board layout after board initialised\n" );
    unsigned char *p = sargon_board;
    for( int i=0; i<12; i++ )
    {
        for( int j=0; j<10; j++ )
            printf( "%02x%c", *p++, j+1<10?' ':'\n' );
    }

    memcpy( sargon_board, test_position, sizeof(board_position) );

    sh.command = 1;
    shim_function( &sh );
    printf( "Board layout after computer move made\n" );
    p = sargon_board;
    for( int i=0; i<12; i++ )
    {
        for( int j=0; j<10; j++ )
            printf( "%02x%c", *p++, j+1<10?' ':'\n' );
    }
    unsigned char *q = sargon_move_made;
    printf( "\nMove is: %c%c-%c%c\n", q[0],q[1],q[2],q[3] );

#endif
    return 0;
}

unsigned char *gen_sargon_format_position( const char *fen )
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

static void convert( std::string fin, std::string asm_fout , std::string report_fout )
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
    std::set<std::string> labels;
    std::map< std::string, std::vector<std::string> > equates;
    std::map< std::string, std::set<std::vector<std::string>> > instructions;
    bool data_mode = true;
    translate_init();

    // Each source line can optionally be transformed to Z80 mnemonics (or hybrid Z80 plus X86 registers mnemonics)
    enum { transform_none, transform_z80, transform_hybrid } transform_switch = transform_hybrid;

    // After optional transformation, the original line can be kept, discarded or commented out
    enum { original_keep, original_comment_out, original_discard } original_switch = original_comment_out;

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
                    asm_line_out = util::sprintf( "%s\tEQU\t%s", stmt.label.c_str(), str_location.c_str() );
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
                        if( stmt.instruction == ".LOC" )
                        {
                            printf( "Error, .LOC not supported. Line: [%s]\n", line_original.c_str() );
                            show_original = true;
                        }
                        else if( stmt.parameters.size() == 0 )
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
                            if( stmt.label != "" )
                            {
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

