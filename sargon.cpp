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

static void convert( std::string fin, std::string fout );

extern "C" {
    int base_function();
}

extern void translate(
    std::ofstream &asm_out,
    const std::string &line,
    const std::string &label,
    const std::string &equate, 
    const std::string &instruction,
    const std::vector<std::string> &parameters );

int main( int argc, const char *argv[] )
{
    util::tests();
#if 1
    convert("sargon-step3.asm","sargon-step3.txt");
#else
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

enum statement_typ {empty, discard, illegal, comment_only, directive, equate, normal};

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
    const char *x86_asm_filename = "mutated.asm";
    std::ofstream asm_out(x86_asm_filename);
    if( !asm_out )
    {
        printf( "Error; Cannot open file %s for writing\n", x86_asm_filename );
        return;
    }
    std::set<std::string> labels;
    std::map< std::string, std::vector<std::string> > equates;
    std::map< std::string, std::set<std::vector<std::string>> > instructions;
    bool skip=false;
    for(;;)
    {
        std::string line;
        if( !std::getline(in,line) )
            break;
        if( line.length()>=5 && line.substr(0,5)=="#if 0" )
            skip = true;
        bool skip_endif=false;
        if( line.length()>=6 && line.substr(0,6)=="#endif" )
        {
            skip = false;
            skip_endif = true;
        }
        if( skip || skip_endif )
            continue;
        util::replace_all(line,"\t"," ");
        std::string original_line = line;
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
                if( offset == 0 || line.length()==0 )
                {
                    stmt.typ = comment_only;
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

        std::string line_out="";
        done = false;
        switch(stmt.typ)
        {
            case empty:
                line_out = "EMPTY"; done=true; break;
            case discard:
                line_out = "DISCARD"; done=true; break;
            case illegal:
                line_out = "ILLEGAL> ";
                line_out += original_line;
                done = true;
                break;
            case comment_only:
                line_out = "COMMENT_ONLY> ";
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
        if( !done )
        {
            translate( asm_out, original_line, stmt.label, stmt.equate, stmt.instruction, stmt.parameters );
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
                line_out += first ? " parameters: " : ", ";
                line_out += "\"";
                line_out += parm;
                line_out += "\"";
                first = false;
            }
        }
        if( !done )
            util::putline(out,line_out);
    }
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
#if 0
    char buf[100];
    extern int simple_function( int x );
    sprintf_s( buf, sizeof(buf), "Force call to skeleton.asm function %d\n", simple_function(34) );
    util::putline(out,buf);
    extern int base_function();
    sprintf_s( buf, sizeof(buf), "Force call to base.asm function %d\n", base_function() );
    util::putline(out,buf);
#endif
}

