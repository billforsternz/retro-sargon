/*

  A program to convert the classic program Sargon, as presented in the book 
  "Sargon a Z80 Computer Chess Program" by Dan and Kathe Spracklen (Hayden Books
  1978) to X86 assembly language. Other programs (/projects) in this suite
  (/repository) exercise the successfully translated code
  
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
#include "translate.h"

void convert( std::string fin, std::string asm_fout,  std::string report_fout, std::string asm_interface_fout );
std::string detabify( const std::string &s, bool push_comment_to_right=false );

// Each source line can optionally be transformed to Z80 mnemonics (or hybrid Z80 plus X86 registers mnemonics)
enum transform_t { transform_none, transform_z80, transform_hybrid };
static transform_t transform_switch = transform_none;

// After optional transformation, the original line can be kept, discarded or commented out
enum original_t { original_keep, original_comment_out, original_discard };
original_t original_switch = original_discard;

// Generated equivalent code can optionally be generated, in various flavours
enum generate_t { generate_x86, generate_z80, generate_hybrid, generate_none };
generate_t generate_switch = generate_x86;

int main( int argc, const char *argv[] )
{
#if 0
    //transform_switch = transform_z80;
    //generate_switch = generate_none;
    //original_switch = original_keep;
    //convert("../original/sargon3.asm","../original/sargon4.asm", "../original/sargon-step4.asm-report.txt", "../original/sargon-step4.asm-asm-interface.h" );
    convert("../original/sargon5.asm","../original/t5-3.asm","../original/translated.asm-report.txt","../original/translated.asm-asm-interface.h");
    return 0;
#endif
    const char *usage=
    "Read, understand, convert sargon source code\n"
    "Usage:\n"
    " convert [switches] sargon.asm sargon-out.asm [report.txt] [asm-interface.h]\n"
    "Switches:\n"
    "Each source line can optionally be transformed to Z80 mnemonics (or hybrid Z80 plus X86 registers mnemonics)\n"
    " so -transform_none or -transform_z80 or -transform_hybrid, default is -transform_none\n"
    "After optional transformation, the original line can be kept, discarded or commented out\n"
    " so -original_keep or -original_comment_out or -original_discard, default is -original_discard\n"
    "Generated equivalent code can optionally be generated, in various flavours\n"
    " so -generate_x86 or -generate_z80 or -generate_hybrid or -generate_none, default is -generate_x86\n"
    "Note that all three output files will be generated, if the optional output filenames aren't\n"
    "provided, names will be auto generated from the main output filename";
    int argi = 1;
    while( argc >= 2)
    {
        std::string arg( argv[argi] );
        if( arg[0] != '-' )
            break;
        else
        {
            if( arg == "-transform_none" )
                transform_switch = transform_none;
            else if( arg == "-transform_z80" )
                transform_switch = transform_z80;
            else if( arg == "-transform_hybrid" )
                transform_switch = transform_hybrid;
            else if( arg == "-original_keep" )
                original_switch = original_keep;
            else if( arg == "-original_discard" )
                original_switch =original_discard;
            else if( arg == "-original_comment_out" )
                original_switch = original_comment_out;
            else if( arg == "-generate_x86" )
                generate_switch = generate_x86;
            else if( arg == "-generate_z80" )
                generate_switch = generate_z80;
            else if( arg == "-generate_hybrid" )
                generate_switch = generate_hybrid;
            else if( arg == "-generate_none" )
                generate_switch = generate_none;
            else
            {
                printf( "Unknown switch %s\n", arg.c_str() );
                printf( "%s\n", usage );
                return -1;
            }
        }
        argc--;
        argi++;
    }
    bool ok = (argc==3 || argc==4 || argc==5);
    if( !ok )
    {
        printf( "%s\n", usage );
        return -1;
    }
    std::string fin ( argv[argi] );
    std::string fout( argv[argi+1] );
    std::string report_fout = argc>=4 ? argv[argi+2] : fout + "-report.txt";
    std::string asm_interface_fout = argc>=5 ? argv[argi+3] : fout + "-asm-interface.h";
    printf( "convert(%s,%s,%s,%s)\n", fin.c_str(), fout.c_str(), report_fout.c_str(), asm_interface_fout.c_str() );
    convert(fin,fout,report_fout,asm_interface_fout);
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

void convert( std::string fin, std::string fout, std::string report_fout, std::string asm_interface_fout )
{
    std::ifstream in(fin);
    if( !in )
    {
        printf( "Error; Cannot open file %s for reading\n", fin.c_str() );
        return;
    }
    std::ofstream asm_out(fout);
    if( !asm_out )
    {
        printf( "Error; Cannot open file %s for writing\n", fout.c_str() );
        return;
    }
    std::ofstream report_out(report_fout);
    if( !report_out )
    {
        printf( "Error; Cannot open file %s for writing\n", report_fout.c_str() );
        return;
    }
    std::ofstream h_out(asm_interface_fout);
    if( !h_out )
    {
        printf( "Error; Cannot open file %s for writing\n", asm_interface_fout.c_str() );
        return;
    }

    util::putline( h_out, "// Automatically generated file - C interface to Sargon assembly language" );
    util::putline( h_out, "extern \"C\" {" );
    util::putline( h_out, "" );
    util::putline( h_out, "    // First byte of Sargon data"  );
    util::putline( h_out, "    extern unsigned char sargon_base_address;" );
    util::putline( h_out, "" );
    util::putline( h_out, "    // // Count generated moves"  );
    util::putline( h_out, "    extern int sargon_move_gen_counter;" );
    util::putline( h_out, "" );
    util::putline( h_out, "    // Calls to sargon() can set and read back registers" );
    util::putline( h_out, "    struct z80_registers" );
    util::putline( h_out, "    {" );
    util::putline( h_out, "        uint16_t af;    // x86 = lo al, hi flags" );
    util::putline( h_out, "        uint16_t hl;    // x86 = bx" );
    util::putline( h_out, "        uint16_t bc;    // x86 = cx" );
    util::putline( h_out, "        uint16_t de;    // x86 = dx" );
    util::putline( h_out, "        uint16_t ix;    // x86 = si" );
    util::putline( h_out, "        uint16_t iy;    // x86 = di" );
    util::putline( h_out, "    };" );
    util::putline( h_out, "" );
    util::putline( h_out, "    // Call Sargon from C, call selected functions, optionally can set input" );
    util::putline( h_out, "    //  registers (and/or inspect returned registers)" );
    util::putline( h_out, "    void sargon( int api_command_code, z80_registers *registers=NULL );" );
    util::putline( h_out, "" );
    util::putline( h_out, "    // Sargon calls C, parameters serves double duty - saved registers on the" );
    util::putline( h_out, "    //  stack, can optionally be inspected by C program" );
    util::putline( h_out, "    void callback( uint32_t edi, uint32_t esi, uint32_t ebp, uint32_t esp," );
    util::putline( h_out, "                   uint32_t ebx, uint32_t edx, uint32_t ecx, uint32_t eax," );
    util::putline( h_out, "                   uint32_t eflags );" );
    util::putline( h_out, "" );
    util::putline( h_out, "    // Data offsets for peeking and poking" );
    bool api_constants_detected = false;
    std::set<std::string> labels;
    std::map< std::string, std::vector<std::string> > equates;
    std::map< std::string, std::set<std::vector<std::string>> > instructions;
    bool data_mode = true;
    translate_init();

    // We can enable (or disable) callback to C++ code
    bool callback_enabled = true;

    // .IF controls let us switch between three modes (currently)
    enum { mode_normal, mode_pass_thru, mode_suspended } mode = mode_normal;

    unsigned int track_location = 0;
    for(;;)
    {
        std::string line;
        if( !std::getline(in,line) )
            break;
        util::rtrim(line);
        std::string line_original = line;
        util::replace_all(line,"\t"," ");
        statement stmt;
        stmt.typ = normal;
        stmt.label = "";
        stmt.equate = "";
        stmt.instruction = "";
        stmt.parameters.clear();
        stmt.comment = "";
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
            util::putline(report_out,line_out);

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

        // Pass through new X86 code
        if( mode==mode_pass_thru && !handled )
        {

            // special handling of lines like "api_n_X:"
            //  generate C code "const int api_X = n;"
            size_t len = line_original.length();
            size_t idx = 0;
            if( len>=8 && line_original.substr(0,4)=="api_" )
            {
                if( line_original[len-1]==':' && '0'<=line_original[4] && line_original[4]<='9' )
                {
                    if( line_original[5] == '_' )
                        idx = 6;
                    else if( '0'<=line_original[5] && line_original[5]<='9' && line_original[6] == '_' )
                        idx = 7;
                }
            }
            if( idx > 0 )
            {
                std::string name = line_original.substr(idx,len-idx-1); // eg "api_1_Y:" len=8, idx=6 -> "Y"
                std::string nbr  = line_original.substr(4,idx==6?1:2);  // eg "api_23_Y:" idx=7 -> "23"
                std::string h_line_out = util::sprintf( "    const int api_%s = %s;", name.c_str(), nbr.c_str() );
                if( !api_constants_detected )
                {
                    api_constants_detected = true;
                    util::putline( h_out, "" );
                    util::putline( h_out, "    // API constants" );
                }
                util::putline( h_out, h_line_out );
            }

            // special handling of line "callback_enabled EQU X"
            std::string line_out = line_original;
            if( line_original=="callback_enabled EQU 0" && callback_enabled )
                line_out = "callback_enabled EQU 1";
            else if( line_original=="callback_enabled EQU 1" && !callback_enabled )
                line_out = "callback_enabled EQU 0";
            util::putline( asm_out, line_out );
            continue;
        }

        if( mode == mode_suspended  )
            continue;

        // Generate assembly language output
        switch( stmt.typ )
        {
            case empty:
                line_original = "";
                line_original = detabify(line_original);
                util::putline( asm_out, line_original );
                break;
            case comment_only:
                line_original = ";" + stmt.comment;
                line_original = detabify(line_original);
                util::putline( asm_out, line_original );
                break;
            case comment_only_indented:
                line_original = "\t;" + stmt.comment;
                line_original = detabify(line_original, generate_switch==generate_x86 );
                util::putline( asm_out, line_original );
                break;
        }
        if( stmt.typ!=normal && stmt.typ!=equate )
            continue;

        if( handled || mode == mode_pass_thru )
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
                    if( stmt.label == "" )
                        line_original = "\t";
                    else
                        line_original = stmt.label + ":\t";
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
                    std::string c_include_line_out = util::sprintf( "    const int %s = 0x%04x;", stmt.label.c_str(), track_location  );
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
                                    asm_line_out += util::sprintf( "\n\tDB\t%d\tDUP (?)\t;Padding bytes to ORG location", accum - track_location );
                                }
                                track_location = accum;
                            }
                            if( stmt.label != "" )
                            {
                                std::string c_include_line_out = util::sprintf( "    const int %s = 0x%04x;", stmt.label.c_str(), track_location  );
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
                {
                    asm_line_out = line_original;
                }
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
            asm_line_out = detabify(asm_line_out, generate_switch==generate_x86 );
            util::putline( asm_out, asm_line_out );
        }
    }
    util::putline( h_out, "};" );

    // Summary report
    util::putline(report_out,"\nLABELS\n");
    for( const std::string &s: labels )
    {
        util::putline(report_out,s);
    }
    util::putline(report_out,"\nEQUATES\n");
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
        util::putline(report_out,s);
    }
    util::putline(report_out,"\nINSTRUCTIONS\n");
    for( const std::pair<std::string,std::set<std::vector<std::string>> > &p: instructions )
    {
        std::string s;
        s += p.first;
        util::putline(report_out,s);
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
            util::putline(report_out,s);
        }
    }
}

std::string detabify( const std::string &s, bool push_comment_to_right )
{
    std::string ret;
    int idx=0;
    int len = s.length();
    for( int i=0; i<len; i++ )
    {
        char c = s[i];
        if( c == '\n' )
        {
            ret += c;
            idx = 0;
        }
        else if( c == '\t' )
        {
            int comment_column   = ( i+1<len && s[i+1]==';' ) ? (push_comment_to_right?48:32) : 0;
            int tab_stops[]      = {8,16,24,32,40,48,100};
            int tab_stops_wide[] = {9,17,25,33,41,49,100};
            bool wide = (original_switch == original_comment_out);
            int *stops = wide ? tab_stops_wide : tab_stops;
            for( int j=0; j<sizeof(tab_stops)/sizeof(tab_stops[0]); j++ )
            {
                if( comment_column>0 || idx<stops[j] )
                {
                    int col = comment_column>0 ? comment_column : stops[j];
                    if( idx >= col )
                        col = idx+1;
                    while( idx < col )
                    {
                        ret += ' ';
                        idx++;
                    }
                    break;
                }
            }
        }
        else
        {
            ret += c;
            idx++;
        }
    }
    return ret;
}
