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
#include "convert-8080-to-z80-or-x86.h"

void convert( bool relax_switch, std::string fin, std::string asm_fout,  std::string report_fout, std::string asm_interface_fout );
std::string detabify( const std::string &s, bool push_comment_to_right=false );

// Each source line can optionally be transformed to Z80 mnemonics (or hybrid Z80 plus X86 registers mnemonics)
enum transform_t { transform_none, transform_z80, transform_hybrid };
static transform_t transform_switch = transform_none;

// After optional transformation, the original line can be kept, discarded or commented out
enum original_t { original_keep, original_comment_out, original_discard };
original_t original_switch = original_discard;

// Generated equivalent code can optionally be generated, in various flavours
enum generate_t { generate_x86, generate_z80, generate_z80_only, generate_hybrid, generate_none };
generate_t generate_switch = generate_x86;

int main( int argc, const char *argv[] )
{
#ifdef _DEBUG
    const char *test_args[] =
    {
        "Release/project-convert-sargon-to-x86.exe",
        "-generate_x86",
        "../stages/sargon-8080-and-x86.asm",
        "../stages/sargon-x86.asm",
        "../stages/sargon-asm-interface.h"
    };
    argc = sizeof(test_args) / sizeof(test_args[0]);
    argv = test_args;
#endif
#if 0 //def _DEBUG
    const char *test_args[] =
    {
        "Release/project-convert-sargon-to-x86.exe",
        "-generate_z80_only",
        "../stages/sargon-8080-and-x86.asm",
        "../stages/sargon-z80.asm"
    };
    argc = sizeof(test_args) / sizeof(test_args[0]);
    argv = test_args;
#endif
#if 0 //def _DEBUG
    const char *test_args[] =
    {
        "Release/project-convert-sargon-to-x86.exe",
        "-generate_z80",
        "../stages/sargon-8080-and-x86.asm",
        "../stages/sargon-z80-and-x86.asm"
    };
    argc = sizeof(test_args) / sizeof(test_args[0]);
    argv = test_args;
#endif
    const char *usage=
    "An 8080 to x86 or Z80 converter/translator. Originally created to read,\n"
    "understand, and convert Sargon source code (which was Z80 code written using an\n"
    "8080 assembler with Z80 extensions). For other projects I expect some work will\n"
    "inevitably be required, C++ source code is freely available on\n"
    "github.com/billforsternz.\n"
    "\n"
    "Usage:\n"
    " convert [switches] sargon.asm sargon-out.asm [asm-interface.h] [report.txt]\n"
    "\n"
    "Switches:\n"
    "\n"
    "Each source line can optionally be transformed to Z80 mnemonics (or hybrid Z80\n"
    "plus X86 registers mnemonics)\n"
    " so -transform_none or -transform_z80 or -transform_hybrid,\n"
    " default is -transform_none\n"
    "\n"
    "After optional transformation, the original line can be kept, discarded or\n"
    "commented out\n"
    " so -original_keep or -original_comment_out or -original_discard,\n"
    " default is -original_discard\n"
    " \n"
    "Generated equivalent code can optionally be generated, in various flavours\n"
    " so -generate_x86 or -generate_z80  or -generate_z80_only or -generate_hybrid\n"
    " or -generate_none, default is -generate_x86. Option -generate_z80_only also\n"
    " strips out .IF_X86 code.\n"
    "\n"
    "Also\n"
    " -relax Relax strict Z80->X86 flag compatibility. Applying this flag eliminates\n"
    "        LAHF/SAHF pairs around some X86 instructions. Reduces compatibility\n"
    "        (burden of proof passes to programmer) but improves performance. For\n"
    "        Sargon, manual checking suggests it's okay to use this flag.\n"
    "\n"
    "Note that all three output files will be generated, if the optional output\n"
    "filenames aren't provided, names will be auto generated from the main output\n"
    "filename.\n";
    int argi = 1;
    bool relax_switch=false;
    while( argc >= 2)
    {
        std::string arg( argv[argi] );
        if( arg[0] != '-' )
            break;
        else
        {
            if( arg == "-relax" )
                relax_switch = true;
            else if( arg == "-transform_none" )
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
            else if( arg == "-generate_z80_only" )
                generate_switch = generate_z80_only;
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
    if( generate_switch == generate_x86 && argc==3 )
    {
        printf( "%s\n", usage );
        printf( "\nIMPORTANT: asm-interface.h is actually required and not optional if -generate_x86\n" );
        return -1;
    }
    std::string fin ( argv[argi] );
    std::string fout( argv[argi+1] );
    std::string asm_interface_fout = argc>=4 ? argv[argi+2] : fout + "-asm-interface.h";
    std::string report_fout = argc>=5 ? argv[argi+3] : fout + "-report.txt";
    convert(relax_switch,fin,fout,report_fout,asm_interface_fout);
    return 0;
}

enum statement_typ {empty, discard, illegal, comment_only, comment_only_indented, equate, normal};

struct statement
{
    statement_typ typ;
    std::string label;
    bool label_has_colon_terminator;
    std::string equate;
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

// Check ASM line syntax - it's not an industrial strength ASM parser, but it's more than
//  sufficient in most cases and will highlight cases where a little manual editing/conversion
//  might be required
// Supported syntax:
//  [label[:]] instruction [parameters,..]
// label and instruction must be alphanum + '_' + '.'
// parameters are less strict, basically anything between commas
// semicolon anywhere initiates comment, so ;rest of line is comment
// complete parameters can be singly ' or " quoted. Commas and semicolons lose their magic in quotes
//
static void parse( const std::string &line, statement &stmt )
{
    enum { init, in_comment, in_label, before_instruction, in_instruction, after_instruction,
           in_parm, in_quoted_parm, in_double_quoted_parm,
           between_parms_before_comma, between_parms_after_comma, err } state;
    state = init;
    std::string parm;
    stmt.typ = normal;
    stmt.label_has_colon_terminator = false;
    for( unsigned int i=0; state!=err && i<line.length(); i++ )
    {
        char c = line[i];
        char next = '\0';
        if( i+1 < line.length() )
            next = line[i+1];
        switch( state )
        {
            case init:
            {
                if( c == ';' )
                {
                    state = in_comment;
                    stmt.typ = comment_only;
                }
                else if( c == ' ' )
                {
                    state = before_instruction;
                }
                else if( c == ':' )
                {
                    state = err;
                }
                else
                {
                    state = in_label;
                    stmt.label = c;
                }
                break;
            }
            case in_comment:
            {
                stmt.comment += c;
                break;
            }
            case in_label:
            {
                if( c == ';' )
                {
                    state = in_comment;
                }
                else if( c == ' ' )
                    state = before_instruction;
                else if( c == ':' )
                {
                    if( next==' ' || next=='\0' )
                    {
                        stmt.label_has_colon_terminator = true;
                        state = before_instruction;
                    }
                    else
                        state = err;
                }
                else
                {
                    stmt.label += c;
                }
                break;
            }
            case before_instruction:
            {
                if( c == ';' )
                {
                    state = in_comment;
                    if( stmt.label == "" )
                        stmt.typ = comment_only_indented;
                }
                else if( c != ' ' )
                {
                    state = in_instruction;
                    stmt.instruction += c;
                }
                break;
            }
            case in_instruction:
            {
                if( c == ';' )
                {
                    state = in_comment;
                }
                else if( c == ' ' )
                {
                    state = after_instruction;
                }
                else
                {
                    stmt.instruction += c;
                }
                break;
            }
            case after_instruction:
            {
                if( c == ';' )
                {
                    state = in_comment;
                }
                else if( c != ' ' )
                {
                    parm = c;
                    if( c == '\'' )
                        state = in_quoted_parm;
                    else if( c == '\"' )
                        state = in_double_quoted_parm;
                    else
                        state = in_parm;
                }
                break;
            }
            case in_parm:
            {
                if( c == ';' )
                {
                    util::rtrim(parm);
                    stmt.parameters.push_back(parm);
                    state = in_comment;
                }
                else if( c == ',' )
                {
                    util::rtrim(parm);
                    stmt.parameters.push_back(parm);
                    state = between_parms_after_comma;
                }
                else
                {
                    parm += c;
                }
                break;
            }
            case in_quoted_parm:
            {
                if( c == '\'' )
                {
                    parm += c;
                    stmt.parameters.push_back(parm);
                    state = between_parms_before_comma;
                }
                else
                {
                    parm += c;
                }
                break;
            }
            case in_double_quoted_parm:
            {
                if( c == '\"' )
                {
                    parm += c;
                    stmt.parameters.push_back(parm);
                    state = between_parms_before_comma;
                }
                else
                {
                    parm += c;
                }
                break;
            }
            case between_parms_before_comma:
            {
                if( c == ';' )
                {
                    state = in_comment;
                }
                else if( c == ',' )
                {
                    state = between_parms_after_comma;
                }
                break;
            }
            case between_parms_after_comma:
            {
                if( c == ',' )
                    state = err;
                else if( c != ' ' )
                {
                    parm = c;
                    if( c == '\'' )
                        state = in_quoted_parm;
                    else if( c == '\"' )
                        state = in_double_quoted_parm;
                    else
                        state = in_parm;
                }
                break;
            }
        }
    }
    const char *identifier_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._";
    if( util::toupper(stmt.label).find_first_not_of(identifier_chars) != std::string::npos )
        stmt.typ = illegal;
    if( stmt.instruction != "=" && util::toupper(stmt.instruction).find_first_not_of(identifier_chars) != std::string::npos )
        stmt.typ = illegal;
    if( state == init )
        stmt.typ = empty;
    else if( state==before_instruction && stmt.label=="" )
        stmt.typ = empty;
    else if( state == err || state==between_parms_after_comma || state==in_quoted_parm || state==in_double_quoted_parm )
        stmt.typ = illegal;
    else if( state == in_parm )
    {
        util::rtrim(parm);
        stmt.parameters.push_back(parm);
    }
    stmt.instruction = util::toupper(stmt.instruction);
    if( stmt.instruction == "=" || util::toupper(stmt.instruction) == "EQU" )
    {
        if( stmt.label == "" || stmt.label_has_colon_terminator || stmt.parameters.size()!=1 )
            stmt.typ = illegal;
        else
        {
            stmt.typ = equate;
            stmt.equate = stmt.label;
            stmt.label = "";
        }
    }
}


void convert( bool relax_switch, std::string fin, std::string fout, std::string report_fout, std::string asm_interface_fout )
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
    util::putline( h_out, "#ifndef SARGON_ASM_INTERFACE_H_INCLUDED" );
    util::putline( h_out, "#define SARGON_ASM_INTERFACE_H_INCLUDED" );
    util::putline( h_out, "extern \"C\" {" );
    util::putline( h_out, "" );
    util::putline( h_out, "    // First byte of Sargon data"  );
    util::putline( h_out, "    extern unsigned char sargon_base_address;" );
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
    std::set<std::string> macros;
    std::map< std::string, std::vector<std::string> > equates;
    std::map< std::string, std::set<std::vector<std::string>> > instructions;
    bool data_mode = true;
    translate_init();
    if( relax_switch )
        translate_init_slim_down();

    // We can enable (or disable) callback to C++ code
    bool callback_enabled = true;

    // .IF controls let us switch between three modes (currently)
    enum { mode_normal, mode_x86, mode_z80, mode_not_z80 } mode = mode_normal;
        // mode_normal, converting 8080 to x86 or z80
        // mode_x86, added x86 code to be passed through (unless generate_z80_only)
        // mode_z80, z80 only code
        //   if generate_z80/generate_z80_only/generate_hybrid convert 8080 -> z80
        //   if generate_x86 remove it
        // mode_not_z80, alternative to z80 code
        //   if generate_z80_only remove it
        //   else same as mode_normal

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
        parse( line, stmt );

        // Reduce to a few simple types of line
        std::string line_out="";
        bool done = false;
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
        bool is_mode_normal = (mode==mode_normal || (generate_switch!=generate_z80_only && mode==mode_not_z80) );
        if( !done && is_mode_normal )
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
        if( !done && is_mode_normal )
            util::putline(report_out,line_out);

        // Handle macro definition
        if( stmt.label != "" && util::toupper(stmt.instruction)=="MACRO" )
            macros.insert(util::toupper(stmt.label));

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
                mode = mode_z80;
                handled = true;         
            }
            else if( stmt.instruction == ".IF_X86" )
            {
                mode = mode_x86;
                handled = true;         
            }
            else if( stmt.instruction == ".ELSE" )
            {
                if( mode==mode_x86 )
                    mode = mode_normal;
                else if( mode==mode_z80 )
                    mode = mode_not_z80;
                else
                    printf( "Error, unexpected .ELSE\n" );
                handled = true;         
            }
            else if( stmt.instruction == ".ENDIF" )
            {
                if( mode==mode_x86 || mode==mode_z80 || mode==mode_not_z80 )
                    mode = mode_normal;
                else
                    printf( "Error, unexpected .ENDIF\n" );
                handled = true;         
            }
        }

        // Our mode switch commands is passed through only if generate_z80
        if( handled )
        {
            if( generate_switch == generate_z80 )
                util::putline( asm_out, line_original );
            continue;
        }

        // In .IF_Z80 mode, discard unless generating z80 code
        if( mode == mode_z80 )
        {
            bool gen_z80 = (generate_switch==generate_z80 || generate_switch==generate_hybrid || generate_switch==generate_z80_only);
            if( !gen_z80 )
                continue;
        }

        // In not .IF_Z80 mode, discard if generating z80 code only
        if( mode == mode_not_z80 )
        {
            if( generate_switch==generate_z80_only )
                continue;
        }

        // Pass through new X86 code unless generate_switch == generate_z80_only
        if( mode==mode_x86 )
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
            if( generate_switch != generate_z80_only )
                util::putline( asm_out, line_out );
            continue;
        }

        // Handle comments
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

        // Handle macro expansion
        bool callback_macro = util::toupper(stmt.instruction)=="CALLBACK";
        if( callback_macro || macros.find(util::toupper(stmt.instruction)) != macros.end() )
        {
            std::string asm_line_out;
            if( stmt.label == "" )
                asm_line_out = "\t";
            else
                asm_line_out = stmt.label + ":\t";
            if( callback_macro && generate_switch==generate_z80_only )
                util::rtrim(asm_line_out);  // Don't express CALLBACK macro if Z80 only
            else
            {
                asm_line_out += stmt.instruction;
                bool first_parm = true;
                for( std::string parm: stmt.parameters )
                {
                    asm_line_out += first_parm ? (callback_macro?" ":"\t") : ",";
                    asm_line_out += parm;
                    first_parm = false;
                }
            }
            if( stmt.comment != "" )
            {
                asm_line_out += "\t;";
                asm_line_out += stmt.comment;
            }
            asm_line_out = detabify(asm_line_out, generate_switch==generate_x86 );
            if( callback_macro && generate_switch==generate_z80_only && stmt.label=="" && stmt.comment=="" )
                ;   // don't express a completely empty line
            else
                util::putline( asm_out, asm_line_out );
            continue;
        }

        // Optionally transform source lines to Z80 mnemonics
        if( transform_switch!=transform_none )
        {
            if( stmt.equate=="" && stmt.instruction!="")
            {
                std::string out;
                bool transformed = translate_z80( line_original, stmt.instruction, stmt.parameters, transform_switch==transform_hybrid, out );
                if( transformed )
                {
                    bool add_colon = (util::toupper(stmt.instruction) != "MACRO");
                    if( !add_colon )
                        printf("debug\n");
                    if( stmt.label == "" )
                        line_original = "\t";
                    else
                        line_original = stmt.label + (add_colon ? ":\t" : "\t");
                    line_original += out;
                    if( stmt.comment != "" )
                    {
                        line_original += "\t;";
                        line_original += stmt.comment;
                    }
                }
            }
        }

        // Keep, discard or comment out source lines before conversion
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
            bool gen_z80 = (generate_switch==generate_z80 || generate_switch==generate_hybrid || generate_switch==generate_z80_only);
            std::string str_location = (gen_z80 ? "$" : util::sprintf( "0%xh", track_location ) );
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
                bool gen_z80 = (generate_switch==generate_z80 || generate_switch==generate_hybrid || generate_switch==generate_z80_only);
                if( gen_z80 )
                {
                    generated = translate_z80( line_original, stmt.instruction, stmt.parameters, generate_switch==generate_hybrid, out );
                    show_original = !generated;
                }
                else
                {
                    if( data_mode && (stmt.instruction == ".LOC" || stmt.instruction == ".BLKB"  || stmt.instruction == ".BLKW"  ||
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
                            else if( stmt.instruction == ".BLKW" )
                            {
                                asm_line_out += util::sprintf( "\tDW\t%s\tDUP (?)", parameter_list.c_str() );
                                unsigned int nbr = atoi( stmt.parameters[0].c_str() );
                                if( nbr == 0 )
                                {
                                    printf( "Error, .BLKW parameter is zero or unparseable. Line: [%s]\n", line_original.c_str() );
                                    show_original = true;
                                }
                                track_location += (2*nbr);
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
                    bool add_colon = !data_mode && (util::toupper(stmt.instruction) != "MACRO");
                    asm_line_out = stmt.label;
                    if( stmt.label == "" )
                        asm_line_out = "\t";
                    else
                        asm_line_out += (add_colon ? ":\t" : "\t");
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
    util::putline( h_out, "#endif //SARGON_ASM_INTERFACE_H_INCLUDED" );

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
