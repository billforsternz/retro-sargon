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


enum ParameterPattern
{
    echo,
    imm8,
    jump_addr_around,
    jump_around,
    mem8,
    mem16,
    set_n_reg8_mem8,
    set_n_reg8,
    clr_n_reg8,
    push_parameter,
    pop_parameter,
    reg8,
    reg16,
    reg8_mem8,
    reg8_mem8_reg8_mem8,
    reg8_mem8_imm8,
    reg8_imm8,
    reg16_imm16,
    none
};

static std::map<const std::string,const char *> xlat1;
static std::map<const std::string,ParameterPattern> xlat2;
static void init();
static std::string detabify( const std::string &s );


bool is_mem8( const std::string &parm, std::string &out )
{
    bool ret = true;
    int len = parm.length();
    if( util::suffix(parm,"(X)" ))
    {
        if( parm[0] == '-' )
            out = "byte ptr [esi-" + parm.substr(1,len-4) + "]";
        else
            out = "byte ptr [esi+" + parm.substr(0,len-3) + "]";
    }
    else if( util::suffix(parm,"(Y)" ))
    {
        if( parm[0] == '-' )
            out = "byte ptr [edi-" + parm.substr(1,len-4) + "]";
        else
            out = "byte ptr [edi+" + parm.substr(0,len-3) + "]";
    }
    else if( isascii(parm[0]) && isalpha(parm[0]) )
    {
        out = "byte ptr ["+ parm +"]";
    }
    else
    {
        ret = false;
    }
    return ret;
}

bool is_mem16( const std::string &parm, std::string &out )
{
    out = "["+ parm +"]";
    return true;
}

bool is_imm16( const std::string &parm, std::string &out, const std::set<std::string> labels )
{
    std::string temp = parm;
    size_t offset = temp.find_first_of("+-");
    if( offset != std::string::npos )
        temp = temp.substr(0,offset);
    auto it = labels.find(temp);
    if( it == labels.end() )
        out = parm;
    else
        out = "offset "+ parm;
    return true;
}

bool is_reg8( const std::string &parm, std::string &out )
{
    bool ret = true;
    if( parm == "A" )
        out = "al";
    else if( parm == "B" )
        out = "ch";
    else if( parm == "C" )
        out = "cl";
    else if( parm == "D" )
        out = "dh";
    else if( parm == "E" )
        out = "dl";
    else if( parm == "H" )
        out = "bh";
    else if( parm == "L" )
        out = "bl";
    else if( parm == "M" )
        out = "byte ptr [ebx]";
    else
        ret = false;
    return ret;
}

bool is_reg16( const std::string &parm, std::string &out )
{
    bool ret = true;
    if( parm == "H" )
        out = "ebx";
    else if( parm == "B" )
        out = "ecx";
    else if( parm == "D" )
        out = "edx";
    else if( parm == "X" )
        out = "esi";
    else if( parm == "Y" )
        out = "edi";
    else
        ret = false;
    return ret;
}

bool is_reg8_mem8( const std::string &parm, std::string &out )
{
    bool ret = true;
    if( !is_reg8(parm,out) )
        ret = is_mem8(parm,out);
    return ret;
}

bool is_imm8( const std::string &parm, std::string &out )
{
    bool ret = true;
    out = parm; // don't actually check anything at least for now
    return ret;
}

bool is_n_set( const std::string &parm, std::string &out )
{
    bool ret = true;
    if( parm=="0" )
        out = "1";
    else if( parm=="1" )
        out = "2";
    else if( parm=="2" )
        out = "4";
    else if( parm=="3" )
        out = "8";
    else if( parm=="4" )
        out = "10h";
    else if( parm=="5" )
        out = "20h";
    else if( parm=="6" )
        out = "40h";
    else if( parm=="7" )
        out = "80h";
    else
        ret = false;
    return ret;
}

bool is_n_clr( const std::string &parm, std::string &out )
{
    bool ret = true;
    if( parm=="0" )
        out = "0feh";
    else if( parm=="1" )
        out = "0fdh";
    else if( parm=="2" )
        out = "0fbh";
    else if( parm=="3" )
        out = "0f7h";
    else if( parm=="4" )
        out = "0efh";
    else if( parm=="5" )
        out = "0dfh";
    else if( parm=="6" )
        out = "0bfh";
    else if( parm=="7" )
        out = "07fh";
    else
        ret = false;
    return ret;
}

static std::set<std::string> labels;
void translate(
    std::ofstream &out,
    const std::string &line,
    const std::string &label,
    const std::string &equate, 
    const std::string &instruction,
    const std::vector<std::string> &parameters )
{
    static int skip_counter=1;
    static bool data_mode=true;
    static bool once=true;
    static bool kill_switch;
    if( kill_switch )
        return;
    if( once )
    {
        init();
        once = false;
        util::putline( out, "	.686P" );
        util::putline( out, "	.XMM");
        util::putline( out, "	include listing.inc" );
        util::putline( out, "	.model	flat" );
        util::putline( out, "" );
        util::putline( out, "CCIR   MACRO ;todo" );
        util::putline( out, "       ENDM" );
        util::putline( out, "PRTBLK MACRO name, len ;todo" );
        util::putline( out, "       ENDM" );
        util::putline( out, "CARRET MACRO ;todo" );
        util::putline( out, "       ENDM" );
        // util::putline( out, "INCLUDELIB OLDNAMES" );
        util::putline( out, "" );
    }
    util::putline(out,";" + line);
    if( label != "" )
        labels.insert(label);
    std::string line_out;
    if( equate != "" )
    {
        line_out = equate;
        line_out += "\tEQU";
        bool first = true;
        for( std::string s: parameters )
        {
            if(first && s[0]=='.')
                s[0]='?';
            line_out += first ? "\t" : ", ";
            first = false;
            line_out += s;
        }
        if( first )
        {
            printf( "Error: No EQU parameters, line=[%s]\n", line.c_str() );
        }
    }
    else if( label != "" && instruction=="" )
    {
        line_out = label;
        line_out += (data_mode?"\tEQU $":":");
    }
    else if( instruction==".DATA" || instruction==".CODE" || instruction==".END" ) // My own invented directives
    {
        data_mode = (instruction==".DATA");
        if( instruction == ".DATA" )
        {
            line_out =
                "_DATA\tSEGMENT";
        }
        else if( instruction == ".CODE" )
        {
            line_out =
                "_DATA\tENDS\n_TEXT\tSEGMENT";
        }
        else if( instruction == ".END" )
        {
            line_out =
                "_TEXT\tENDS\nEND";
            kill_switch = true;
        }
    }
    else 
    {
        if( label != "" )
        {
            line_out = label;
            line_out += (data_mode?"":":");
        }
        line_out += "\t";
        auto it1 = xlat1.find(instruction.c_str());
        if( it1 == xlat1.end() )
        {
            printf( "Error: Unknown instruction %s, line=[%s]\n", instruction.c_str(), line.c_str() );
            return;
        }
        const char *format = it1->second;
        auto it2 = xlat2.find(instruction.c_str());
        if( it2 == xlat2.end() )
        {
            printf( "Error: Unknown instruction %s, line=[%s]\n", instruction.c_str(), line.c_str() );
            return;
        }
        ParameterPattern pp = it2->second;
        std::string parameters_listed;
        bool first = true;
        for( const std::string s: parameters )
        {
            parameters_listed += first ? "" : ", ";
            first = false;
            parameters_listed += s;
        }
        std::string xlat(format);
        xlat += "\t";
        xlat += parameters_listed;
        xlat += " (not yet)";
        if( pp!=none && pp!=jump_around && parameters.size()<1 )
        {
            printf( "Error: Expect at least one parameter, instruction=[%s], line=[%s]\n", instruction.c_str(), line.c_str() );
            return;
        }
        switch(pp)
        {
            default:
            {
                printf( "Error: Unknown parameter pattern, instruction=[%s], line=[%s]\n", instruction.c_str(), line.c_str() );
                return;
            }
            case echo:
            {
                xlat = util::sprintf( format, parameters_listed.c_str() );
                break;
            }
            case imm8:
            {
                std::string parm = parameters[0];
                std::string out;
                if( is_imm8(parm,out) )
                    xlat = util::sprintf( format, out.c_str() );
                else
                {
                    printf( "Error: Illegal imm8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                break;
            }
            case jump_addr_around:
            {
                std::string temp = util::sprintf( "skip%d", skip_counter++ );
                xlat = util::sprintf( format, temp.c_str(), parameters[0].c_str(), temp.c_str() );
                break;
            }
            case jump_around:
            {
                std::string temp = util::sprintf( "skip%d", skip_counter++ );
                xlat = util::sprintf( format, temp.c_str(), temp.c_str() );
                break;
            }
            case mem8:
            {
                std::string parm = parameters[0];
                std::string out;
                if( is_mem8(parm,out) )
                    xlat = util::sprintf( format, out.c_str() );
                else
                {
                    printf( "Error: Illegal mem8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                break;
            }
            case mem16:
            {
                std::string parm = parameters[0];
                std::string out;
                if( is_mem16(parm,out) )
                    xlat = util::sprintf( format, out.c_str() );
                else
                {
                    printf( "Error: Illegal mem16 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                break;
            }
            case set_n_reg8_mem8:
            {
                if( parameters.size() < 2 )
                {
                    printf( "Error: Need two parameters, line=[%s]\n", line.c_str() );
                    return;
                }
                std::string parm = parameters[0];
                std::string out1;
                if( !is_n_set(parm,out1) )
                {
                    printf( "Error: Unknown n parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                parm = parameters[1];
                std::string out2;
                if( is_reg8_mem8(parm,out2) )
                    xlat = util::sprintf( format, out2.c_str(), out1.c_str() );     // note reverse order
                else
                {
                    printf( "Error: Illegal reg8_mem8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                break;
            }
            case set_n_reg8:
            {
                if( parameters.size() < 2 )
                {
                    printf( "Error: Need two parameters, line=[%s]\n", line.c_str() );
                    return;
                }
                std::string parm = parameters[0];
                std::string out1;
                if( !is_n_set(parm,out1) )
                {
                    printf( "Error: Unknown n parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                parm = parameters[1];
                std::string out2;
                if( is_reg8(parm,out2) )
                    xlat = util::sprintf( format, out2.c_str(), out1.c_str() );     // note reverse order
                else
                {
                    printf( "Error: Illegal reg8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                break;
            }
            case clr_n_reg8:
            {
                if( parameters.size() < 2 )
                {
                    printf( "Error: Need two parameters, line=[%s]\n", line.c_str() );
                    return;
                }
                std::string parm = parameters[0];
                std::string out1;
                if( !is_n_clr(parm,out1) )
                {
                    printf( "Error: Unknown n parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                parm = parameters[1];
                std::string out2;
                if( is_reg8(parm,out2) )
                    xlat = util::sprintf( format, out2.c_str(), out1.c_str() );     // note reverse order
                else
                {
                    printf( "Error: Illegal reg8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                break;
            }
            case push_parameter:
            {
                std::string parm = parameters[0];
                std::string out;
                if( parm == "PSW" )
                    xlat = "lahf\n\tPUSH eax";
                else if( is_reg16(parm,out) )
                    xlat = util::sprintf( "PUSH %s", out.c_str() );
                else
                {
                    printf( "Error: Illegal push parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                break;
            }
            case pop_parameter:
            {
                std::string parm = parameters[0];
                std::string out;
                if( parm == "PSW" )
                    xlat = "POP eax\n\tsahf";
                else if( is_reg16(parm,out) )
                    xlat = util::sprintf( "POP %s", out.c_str() );
                else
                {
                    printf( "Error: Illegal pop parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                break;
            }
            case reg8:
            {
                std::string parm = parameters[0];
                std::string out;
                if( is_reg8(parm,out) )
                    xlat = util::sprintf( format, out.c_str() );
                else
                {
                    printf( "Error: Unknown reg8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                break;
            }
            case reg16:
            {
                std::string parm = parameters[0];
                std::string out;
                if( is_reg16(parm,out) )
                    xlat = util::sprintf( format, out.c_str() );
                else
                {
                    printf( "Error: Unknown reg16 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                break;
            }
            case reg8_mem8:
            {
                std::string parm = parameters[0];
                std::string out;
                if( is_reg8_mem8(parm,out) )
                    xlat = util::sprintf( format, out.c_str() );
                else
                {
                    printf( "Error: Unknown reg8_mem8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                break;
            }
            case reg8_mem8_reg8_mem8:
            {
                if( parameters.size() < 2 )
                {
                    printf( "Error: Need two parameters, line=[%s]\n", line.c_str() );
                    return;
                }
                std::string parm = parameters[0];
                std::string out1;
                if( !is_reg8_mem8(parm,out1) )
                {
                    printf( "Error: Unknown reg8_mem8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                parm = parameters[1];
                std::string out2;
                if( is_reg8_mem8(parm,out2) )
                    xlat = util::sprintf( format, out1.c_str(), out2.c_str() );
                else
                {
                    printf( "Error: Unknown reg8_mem8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                break;
            }
            case reg8_mem8_imm8:
            {
                if( parameters.size() < 2 )
                {
                    printf( "Error: Need two parameters, line=[%s]\n", line.c_str() );
                    return;
                }
                std::string parm = parameters[0];
                std::string out1;
                if( !is_reg8_mem8(parm,out1) )
                {
                    printf( "Error: Unknown reg8_mem8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                parm = parameters[1];
                std::string out2;
                if( is_imm8(parm,out2) )
                    xlat = util::sprintf( format, out1.c_str(), out2.c_str() );
                else
                {
                    printf( "Error: Unknown imm8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                break;
            }
            case reg8_imm8:
            {
                if( parameters.size() < 2 )
                {
                    printf( "Error: Need two parameters, line=[%s]\n", line.c_str() );
                    return;
                }
                std::string parm = parameters[0];
                std::string out1;
                if( !is_reg8(parm,out1) )
                {
                    printf( "Error: Unknown reg8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                parm = parameters[1];
                std::string out2;
                if( is_imm8(parm,out2) )
                    xlat = util::sprintf( format, out1.c_str(), out2.c_str() );
                else
                {
                    printf( "Error: Illegal imm8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                break;
            }
            case reg16_imm16:
            {
                if( parameters.size() < 2 )
                {
                    printf( "Error: Need two parameters, line=[%s]\n", line.c_str() );
                    return;
                }
                std::string parm = parameters[0];
                std::string out1;
                if( !is_reg16(parm,out1) )
                {
                    printf( "Error: Unknown reg16 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                parm = parameters[1];
                std::string out2;
                if( is_imm16(parm,out2,labels) )
                    xlat = util::sprintf( format, out1.c_str(), out2.c_str() );
                else
                {
                    printf( "Error: Illegal imm16 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                    return;
                }
                break;
            }
            case none:
            {
                xlat = std::string(format);
                break;
            }
        }
        line_out += xlat;
    }
    std::string s = detabify(line_out);
    util::putline(out,s);
}

static std::string detabify( const std::string &s )
{
    std::string ret;
    int idx=0;
    for( char c: s )
    {
        if( c == '\n' )
        {
            ret += c;
            idx = 0;
        }
        else if( c == '\t' )
        {
            int tab_stops[] = {9,17,25,33,41,49,100};
            for( int i=0; i<sizeof(tab_stops)/sizeof(tab_stops[0]); i++ )
            {
                if( idx < tab_stops[i] )
                {
                    while( idx < tab_stops[i] )
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

static void init()
{
    //
    // 8 bit register move
    //

    // MOV reg8_mem8, reg8_mem8 -> MOV reg8_mem8, reg8_mem8
    xlat1["MOV"] = "MOV %s,%s";
    xlat2["MOV"] = reg8_mem8_reg8_mem8;

    // MVI reg8_mem8, imm8 -> MOV reg8_mem8, imm8
    xlat1["MVI"] = "MOV %s,%s";
    xlat2["MVI"] = reg8_mem8_imm8;

    //
    // Push and pop
    //

    // PUSH reg16 -> PUSH reg32
    // PUSH PSW -> LAHF; PUSH AX
    xlat1["PUSH"] = "%s";
    xlat2["PUSH"] = push_parameter;

    // POP reg16 -> POP reg32
    // POP PSW -> POP AX; SAHF
    xlat1["POP"] = "%s";
    xlat2["POP"] = pop_parameter;

    //
    // 8 bit arithmetic and logic
    //

    // ADD reg8_mem8 -> ADD al,reg8_mem8
    xlat1["ADD"] = "ADD al,%s";
    xlat2["ADD"] = reg8_mem8;

    // ADI imm8 -> ADD al,imm8
    xlat1["ADI"] = "ADD al,%s";
    xlat2["ADI"] = imm8;

    // ANA reg8_mem8 -> AND al,reg8_mem8
    xlat1["ANA"] = "AND al,%s";
    xlat2["ANA"] = reg8_mem8;

    // ANI imm8 -> AND al,imm8
    xlat1["ANI"] = "AND al,%s";
    xlat2["ANI"] = imm8;

    // SUB reg8_mem8 -> SUB al,reg8_mem8
    xlat1["SUB"] = "SUB al,%s";
    xlat2["SUB"] = reg8_mem8;

    // SUI imm8 -> SUB al,imm8
    xlat1["SUI"] = "SUB al,%s";
    xlat2["SUI"] = imm8;

    // XRA  reg8_mem8 -> XOR al,reg8_mem8
    xlat1["XRA"] = "XOR al,%s";
    xlat2["XRA"] = reg8_mem8;

    // XRI imm8 -> XOR al,imm8
    xlat1["XRI"] = "XOR al,%s";
    xlat2["XRI"] = imm8;

    // CMP reg8_mem8 -> CMP al,reg8_mem8
    xlat1["CMP"] = "CMP al,%s";
    xlat2["CMP"] = reg8_mem8;

    // CPI imm8 -> CMP al, imm8
    xlat1["CPI"] = "CMP al,%s";
    xlat2["CPI"] = imm8;

    // DCR reg8 -> DEC reg8
    xlat1["DCR"] = "DEC %s";
    xlat2["DCR"] = reg8;

    // INR reg8 -> INC reg8
    xlat1["INR"] = "INC %s";
    xlat2["INR"] = reg8;

    //
    // 16 bit arithmetic
    //

    // DAD reg16 -> LAHF; ADD ebx,reg16; SAHF        //CY (only) should be affected, but our emulation preserves flags
    xlat1["DAD"] = "LAHF\n\tADD ebx,%s\n\tSAHF";
    xlat2["DAD"] = reg16;

    // DADX reg16 -> LAHF; ADD esi,reg16; SAHF       //CY (only) should be affected, but our emulation preserves flags
    xlat1["DADX"] = "LAHF\n\tADD esi,%s\n\tSAHF";
    xlat2["DADX"] = reg16;

    // DADY reg16 -> LAHF; ADD edi,reg16; SAHF;      //CY (only) should be affected, but our emulation preserves flags
    xlat1["DADY"] = "LAHF\n\tADD edi,%s\n\tSAHF";    // not actually used in codebase
    xlat2["DADY"] = reg16;

    // DSBC reg16 -> SBB ebx,reg16
    xlat1["DSBC"] = "SBB ebx,%s";
    xlat2["DSBC"] = reg16;

    // INX reg16 -> LAHF; INC reg16; SAHF;      ## INC reg16; Z80 flags unaffected, X86 INC preserve CY only
    xlat1["INX"] = "LAHF\n\tINC %s\n\tSAHF";
    xlat2["INX"] = reg16;

    // DCX reg16 -> LAHF; DEC reg16; SAHF;      ## DEC reg16; Z80 flags unaffected, X86 DEC preserve CY only
    xlat1["DCX"] = "LAHF\n\tDEC %s\n\tSAHF";
    xlat2["DCX"] = reg16;

    //
    // Bit test, set, clear
    //

    // BIT n,reg8_mem8 -> MOV ah,reg8_mem8; AND ah,mask(n) # but damages other flags
    xlat1["BIT"] = "MOV ah,%s\n\tAND ah,%s";
    xlat2["BIT"] = set_n_reg8_mem8;

    // SET n,reg8 -> LAHF; OR reg8,mask[n]; SAHF
    xlat1["SET"] = "LAHF\n\tOR %s,%s\n\tSAHF";
    xlat2["SET"] = set_n_reg8;

    // RES n,reg8 -> LAHF; AND reg8,not mask[n]; SAHF
    xlat1["RES"] = "LAHF\n\tAND %s,%s\n\tSAHF";
    xlat2["RES"] = clr_n_reg8;

    //
    // Rotate and Shift
    //

    // RAL -> RCL al,1             # rotate left through CY
    xlat1["RAL"] = "RCL al,1";
    xlat2["RAL"] = none;

    // RARR reg8 -> RCR reg, 1     # rotate right through CY
    xlat1["RARR"] = "RCR %s, 1";
    xlat2["RARR"] = reg8;

    // RLD -> macro or call; 12 bits of low AL and byte [BX] rotated 4 bits left (!!)
    xlat1["RLD"] = "CALL Z80_RLD";
    xlat2["RLD"] = none;

    // RRD -> macro or call; 12 bits of low AL and byte [BX] rotated 4 bits right (!!)
    xlat1["RRD"] = "CALL Z80_RRD";
    xlat2["RRD"] = none;

    // SLAR reg8 -> SHL reg8,1     # left shift into CY, bit 0 zeroed (arithmetic and logical are the same)
    xlat1["SLAR"] = "SHL %s,1";
    xlat2["SLAR"] = reg8;

    // SRAR reg8 -> SAR reg8,1     # arithmetic right shift into CY, bit 7 preserved
    xlat1["SRAR"] = "SAR %s,1";
    xlat2["SRAR"] = reg8;

    // SRLR reg8 -> SHR reg8,1     # logical right shift into CY, bit 7 zeroed
    xlat1["SRLR"] = "SHR %s,1";
    xlat2["SRLR"] = reg8;

    //
    // Calls
    //

    // CALL addr -> CALL addr
    xlat1["CALL"] = "CALL %s";
    xlat2["CALL"] = echo;

    // CC addr -> JNC temp; CALL addr; temp:
    xlat1["CC"] = "JNC %s\n\tCALL %s\n%s:";
    xlat2["CC"] = jump_addr_around;

    // CNZ addr -> JZ temp; CALL addr; temp:
    xlat1["CNZ"] = "JZ %s\n\tCALL %s\n%s:";
    xlat2["CNZ"] = jump_addr_around;

    // CZ addr -> JNZ temp; CALL addr; temp:
    xlat1["CZ"] = "JNZ %s\n\tCALL %s\n%s:";
    xlat2["CZ"] = jump_addr_around;

    //
    // Returns
    //

    // RET -> RET
    xlat1["RET"] = "RET";
    xlat2["RET"] = none;

    // RC -> JNC temp; RET; temp:
    xlat1["RC"] = "JNC %s\n\tRET\n%s:";
    xlat2["RC"] = jump_around;

    // RNC -> JC temp; RET; temp:
    xlat1["RNC"] = "JC %s\n\tRET\n%s:";
    xlat2["RNC"] = jump_around;

    // RNZ -> JZ temp; RET; temp:
    xlat1["RNZ"] = "JZ %s\n\tRET\n%s:";
    xlat2["RNZ"] = jump_around;

    // RZ -> JNZ temp; RET; temp:
    xlat1["RZ"] = "JNZ %s\n\tRET\n%s:";
    xlat2["RZ"] = jump_around;

    //
    // Jumps
    //

    // DJNZ addr -> LAHF; DEC ch; JNZ addr; SAHF; ## flags affected at addr (sadly not much to be done)
    xlat1["DJNZ"] = "LAHF\n\tDEC ch\n\tJNZ %s\n\tSAHF";
    xlat2["DJNZ"] = echo;

    // JC addr -> JC addr
    xlat1["JC"] = "JC %s";
    xlat2["JC"] = echo;

    // JM addr -> JS addr  (jump if sign bit true = most sig bit true = negative)
    xlat1["JM"] = "JS %s";
    xlat2["JM"] = echo;

    // JMP addr -> JMP addr
    xlat1["JMP"] = "JMP %s";
    xlat2["JMP"] = echo;

    // JMPR addr -> JMP addr (avoid relative jumps unless we get into mega optimisation)
    xlat1["JMPR"] = "JMP %s";
    xlat2["JMPR"] = echo;

    // JNZ addr -> JNZ addr
    xlat1["JNZ"] = "JNZ %s";
    xlat2["JNZ"] = echo;

    // JP addr -> JP addr  (jump sign positive)
    xlat1["JP"] = "JP %s";
    xlat2["JP"] = echo;

    // JPE addr -> JPE addr  (jump parity even - check this one, parity bit not 100% compatible) 
    xlat1["JPE"] = "JPE %s";
    xlat2["JPE"] = echo;

    // JRC addr -> JC addr (avoid relative jumps unless we get into mega optimisation)
    xlat1["JRC"] = "JC %s";
    xlat2["JRC"] = echo;

    // JRNC addr -> JNC addr
    xlat1["JRNC"] = "JNC %s";
    xlat2["JRNC"] = echo;

    // JRNZ addr -> JNZ addr
    xlat1["JRNZ"] = "JNZ %s";
    xlat2["JRNZ"] = echo;

    // JRZ addr -> JZ addr
    xlat1["JRZ"] = "JZ %s";
    xlat2["JRZ"] = echo;

    // JZ addr -> JZ addr
    xlat1["JZ"] = "JZ %s";
    xlat2["JZ"] = echo;

    //
    // Load memory -> register
    //

    // LDA mem8 -> MOV AL,mem8
    xlat1["LDA"] = "MOV AL,%s";
    xlat2["LDA"] = mem8;

    // LDAX reg16 -> MOV al,[reg16]
    xlat1["LDAX"] = "MOV al,[%s]";
    xlat2["LDAX"] = reg16;

    // LHLD mem16 -> MOV ebx,[mem16]
    xlat1["LHLD"] = "MOV ebx,%s";
    xlat2["LHLD"] = mem16;

    // LBCD mem16 -> MOV ecx,[mem16]
    xlat1["LBCD"] = "MOV ecx,%s";
    xlat2["LBCD"] = mem16;

    // LDED mem16 -> MOV edx,[mem16]
    xlat1["LDED"] = "MOV edx,%s";
    xlat2["LDED"] = mem16;

    // LIXD mem16 -> MOV esi,[mem16]
    xlat1["LIXD"] = "MOV esi,%s";
    xlat2["LIXD"] = mem16;

    // LIYD mem16 -> MOV edi,[mem16]
    xlat1["LIYD"] = "MOV edi,%s";
    xlat2["LIYD"] = mem16;

    // LXI reg16, imm16 -> MOV reg16, imm16 (if imm16 is a label precede it with offset)
    xlat1["LXI"] = "MOV %s,%s";
    xlat2["LXI"] = reg16_imm16;

    //
    // Store register -> memory
    //

    // STA mem8 -> MOV [mem8],al
    xlat1["STA"]  = "MOV %s,al";
    xlat2["STA"]  = mem8;

    // SHLD mem16 -> mov [mem16],ebx
    xlat1["SHLD"] = "MOV %s,ebx";
    xlat2["SHLD"] = mem16;

    // SBCD mem16 -> MOV [mem16],ecx
    xlat1["SBCD"] = "MOV %s,ecx";
    xlat2["SBCD"] = mem16;

    // SDED mem16 -> mov [mem16],edx
    xlat1["SDED"] = "MOV %s,edx";
    xlat2["SDED"] = mem16;

    // SIXD mem16 -> mov [mem16],esi
    xlat1["SIXD"] = "MOV %s,esi";
    xlat2["SIXD"] = mem16;

    //
    // Miscellaneous
    //

    // NEG -> NEG al
    xlat1["NEG"] = "NEG al";
    xlat2["NEG"] = none;

    // XCHG -> XCHG ebx,edx
    xlat1["XCHG"] = "XCHG ebx,edx";
    xlat2["XCHG"] = none;

    // EXAF -> macro/call
    xlat1["EXAF"] = "CALL Z80_EXAF";
    xlat2["EXAF"] = none;

    // EXX -> macro/call
    xlat1["EXX"] = "CALL Z80_EXX";
    xlat2["EXX"] = none;

    // LDAR -> Load A with incrementing R (RAM refresh) register ????
    xlat1["LDAR"] = "CALL Z80_LDAR";
    xlat2["LDAR"] = none;

    //
    // Macros
    //
    xlat1["CCIR"] = "CCIR";
    xlat2["CCIR"] = none;
    xlat1["CARRET"] = "CARRET";
    xlat2["CARRET"] = none;
    xlat1["PRTBLK"] = "PRTBLK %s";
    xlat2["PRTBLK"] = echo;

    //
    // Directives
    //
    xlat1[".BLKB"] = "DB %s DUP (?)";
    xlat2[".BLKB"] = echo;
    xlat1[".BYTE"] = "DB %s";
    xlat2[".BYTE"] = echo;
    xlat1[".WORD"] = "DD %s";
    xlat2[".WORD"] = echo;
    xlat1[".LOC"] = "ORG %s";
    xlat2[".LOC"] = echo;
}

