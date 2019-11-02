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

static void convert( std::string fin, std::string fout );

struct shim_parameters
{
    unsigned char *board;
};

extern "C" {
    int shim_function( shim_parameters *parm );
}

int main( int argc, const char *argv[] )
{
    util::tests();
#if 0
    shim_parameters sh;
    shim_function( &sh );
    const unsigned char *p = sh.board;
    printf( "Board layout\n" );
    for( int i=0; i<12; i++ )
    {
        for( int j=0; j<10; j++ )
            printf( "%02x%c", *p++, j+1<10?' ':'\n' );
    }
#endif

#if 1
    convert("sargon-step5.asm","sargon-step5.txt");
#endif

#if 0
    bool ok = (argc==3);
    if( !ok )
    {
        printf(
            "Read and understand sargon source code\n"
            "Usage:\n"
            " convert sargon.asm output.txt\n"
        );
        return -1;
    }
    convert(argv[1],argv[2]);
#endif
    return 0;
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

static void convert( std::string fin, std::string fout )
{
    std::ifstream in(fin);
    if( !in )
    {
        printf( "Error; Cannot open file %s for reading\n", fin.c_str() );
        return;
    }
    std::ofstream out(fout);
    if( !out )
    {
        printf( "Error; Cannot open file %s for writing\n", fout.c_str() );
        return;
    }
    const char *x86_asm_filename = "output-step5.asm";
    std::ofstream asm_out(x86_asm_filename);
    if( !asm_out )
    {
        printf( "Error; Cannot open file %s for writing\n", x86_asm_filename );
        return;
    }
    std::set<std::string> labels;
    std::map< std::string, std::vector<std::string> > equates;
    std::map< std::string, std::set<std::vector<std::string>> > instructions;
    bool data_mode = true;
    bool suspended = false;
    translate_init();

    // Each source line can optionally be transformed to Z80 mnemonics (or hybrid Z80 plus X86 registers mnemonics)
    enum { transform_none, transform_z80, transform_hybrid } transform_switch = transform_hybrid;

    // After optional transformation, the original line can be kept, discarded or commented out
    enum { original_keep, original_comment_out, original_discard } original_switch = original_comment_out;

    // Generated equivalent code can optionally be generated, in various flavours
    enum { generate_x86, generate_z80, generate_hybrid, generate_none } generate_switch = generate_x86;

    for(;;)
    {
        std::string line;
        if( !std::getline(in,line) )
            break;
        std::string original_line = line;
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
                line_out += original_line;
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
        if( !done )
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
        if( !done )
            util::putline(out,line_out);

        // Generate assembly language output
        switch( stmt.typ )
        {
            case empty:
            case comment_only:
            case comment_only_indented:
                original_line = detabify(original_line);
                util::putline( asm_out, original_line );
                break;
        }
        if( stmt.typ!=normal && stmt.typ!=equate )
            continue;

        // My invented directives
        bool handled=false;
        if( stmt.label=="" )
        {
            if( stmt.instruction == ".BEGIN" )
            {
                util::putline( asm_out, detabify("\t.686P") );
                util::putline( asm_out, detabify("\t.XMM") );
                util::putline( asm_out, detabify("\t.model\tflat") );
                util::putline( asm_out, detabify("") );
                util::putline( asm_out, detabify("CCIR\tMACRO\t;todo") );
                util::putline( asm_out, detabify("\tENDM") );
                util::putline( asm_out, detabify("PRTBLK\tMACRO\tname,len\t;todo") );
                util::putline( asm_out, detabify("\tENDM") );
                util::putline( asm_out, detabify("CARRET\tMACRO\t;todo") );
                util::putline( asm_out, detabify("\tENDM") );
                util::putline( asm_out, detabify("") );
                util::putline( asm_out, detabify("Z80_EXAF\tMACRO") );
                util::putline( asm_out, detabify("\tlahf") );
                util::putline( asm_out, detabify("\txchg\teax,shadow_eax") );
                util::putline( asm_out, detabify("\tsahf") );
                util::putline( asm_out, detabify("\tENDM") );
                util::putline( asm_out, detabify("") );
                util::putline( asm_out, detabify("Z80_EXX\tMACRO") );
                util::putline( asm_out, detabify("\txchg\tebx,shadow_ebx") );
                util::putline( asm_out, detabify("\txchg\tecx,shadow_ecx") );
                util::putline( asm_out, detabify("\txchg\tedx,shadow_edx") );
                util::putline( asm_out, detabify("\tENDM") );
                util::putline( asm_out, detabify("") );
                util::putline( asm_out, detabify("Z80_RLD\tMACRO\t;a=kx (hl)=yz -> a=ky (hl)=zx") );
                util::putline( asm_out, detabify("\tmov\tah,byte ptr [ebx]\t;ax=yzkx") );
                util::putline( asm_out, detabify("\tror\tal,4\t;ax=yzxk") );
                util::putline( asm_out, detabify("\trol\tax,4\t;ax=zxky") );
                util::putline( asm_out, detabify("\tmov\tbyte ptr [ebx],ah\t;al=ky [ebx]=zx") );
                util::putline( asm_out, detabify("\tor\tal,al\t;set z and s flags") );
                util::putline( asm_out, detabify("\tENDM") );
                util::putline( asm_out, detabify("") );
                util::putline( asm_out, detabify("Z80_RRD\tMACRO\t;a=kx (hl)=yz -> a=kz (hl)=xy") );
                util::putline( asm_out, detabify("\tmov\tah,byte ptr [ebx]\t;ax=yzkx") );
                util::putline( asm_out, detabify("\tror\tax,4\t;ax=xyzk") );
                util::putline( asm_out, detabify("\tror\tal,4\t;ax=xykz") );
                util::putline( asm_out, detabify("\tmov\tbyte ptr [ebx],ah\t;al=kz [ebx]=xy") );
                util::putline( asm_out, detabify("\tor\tal,al\t;set z and s flags") );
                util::putline( asm_out, detabify("\tENDM") );
                util::putline( asm_out, detabify("") );
                util::putline( asm_out, detabify("Z80_LDAR\tMACRO\t;to get random number") );
                util::putline( asm_out, detabify("\tpushf\t;maybe there's entropy in stack junk") );
                util::putline( asm_out, detabify("\tpush\tebx") );
                util::putline( asm_out, detabify("\tmov\tbx,bp") );
                util::putline( asm_out, detabify("\tmov\tax,0") );
                util::putline( asm_out, detabify("\txor\tal,byte ptr [ebx]") );
                util::putline( asm_out, detabify("\tdec\tbx") );
                util::putline( asm_out, detabify("\tjz\t$+4") );
                util::putline( asm_out, detabify("\tdec\tah") );
                util::putline( asm_out, detabify("\tjnz\t$-10") );
                util::putline( asm_out, detabify("\tpop\tebx") );
                util::putline( asm_out, detabify("\tpopf") );
                util::putline( asm_out, detabify("\tENDM") );
                util::putline( asm_out, detabify("") );
                handled = true;         
            }
            else if( stmt.instruction == ".SUSPEND" )
            {
                suspended = true;
                handled = true;
            }
            else if( stmt.instruction == ".RESUME" )
            {
                suspended = false;
                handled = true;
            }
            else if( stmt.instruction == ".DATA" )
            {
                data_mode = true;
                util::putline( asm_out, detabify("_DATA\tSEGMENT") );
                util::putline( asm_out, "" );
                handled = true;
            }
            else if( stmt.instruction == ".CODE" )
            {
                data_mode = false;
                util::putline( asm_out, "shadow_eax  dd   0" );
                util::putline( asm_out, "shadow_ebx  dd   0" );
                util::putline( asm_out, "shadow_ecx  dd   0" );
                util::putline( asm_out, "shadow_edx  dd   0" );
                util::putline( asm_out, detabify("_DATA\tENDS\n_TEXT\tSEGMENT") );
                util::putline( asm_out, "" );
                handled = true;
            }
            else if( stmt.instruction == ".END" )
            {
                suspended = true;
                util::putline( asm_out, detabify("_TEXT\tENDS\nEND") );
                util::putline( asm_out, "" );
                handled = true;
            }
        }
        if( handled || suspended )
            continue;

        // Optionally transform source lines to Z80 mnemonics
        if( transform_switch!=transform_none )
        {
            if( stmt.equate=="" && stmt.instruction!="")
            {
                std::string out;
                bool transformed = translate_z80( original_line, stmt.instruction, stmt.parameters, transform_switch==transform_hybrid, out );
                if( transformed )
                {
                    original_line = stmt.label;
                    if( stmt.label == "" )
                        original_line = "\t";
                    else
                        original_line += (data_mode?"\t":":\t");
                    original_line += out;
                    if( stmt.comment != "" )
                    {
                        original_line += "\t;";
                        original_line += stmt.comment;
                    }
                }
            }
        }
        switch( original_switch )
        {
            case original_comment_out:
            {
                util::putline( asm_out, detabify( ";" + original_line) );
                break;
            }
            case original_keep:
            {
                util::putline( asm_out, detabify(original_line) );
                break;
            }
            default:
            case original_discard:
                break;
        }

        // Optionally generate code
        if( generate_switch != generate_none )
        {
            std::string asm_line_out;
            if( stmt.equate != "" )
            {
                asm_line_out = stmt.equate;
                asm_line_out += "\tEQU";
                bool first = true;
                for( std::string s: stmt.parameters )
                {
                    if(first && s[0]=='.')
                        s[0]='?';
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
                asm_line_out = stmt.label;
                asm_line_out += (data_mode?"\tEQU $":":");
            }
            else if( stmt.instruction!="" )
            {
                std::string out;
                bool generated = false;
                if( generate_switch == generate_x86 )
                    generated = translate_x86( original_line, stmt.instruction, stmt.parameters, labels, out );
                else
                    generated = translate_z80( original_line, stmt.instruction, stmt.parameters, generate_switch==generate_hybrid, out );
                if( !generated )
                    asm_line_out = original_line;
                else
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
                                asm_line_out += out.substr(offset);
                            }
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

