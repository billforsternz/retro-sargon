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
    reg16_imm16,
    rst_z80,
    none
};

static int skip_counter=1;

struct MnemonicConversion
{
    const char *x86;
    const char *z80;
    const char *hybrid;
    ParameterPattern pp;
};

static std::map<std::string,MnemonicConversion> xlat;


bool is_mem8( const std::string &parm, std::string &out )
{
    bool ret = true;
    int len = parm.length();
    if( util::suffix(parm,"(X)" ))
    {
        if( parm[0] == '-' )
            out = "byte ptr [ebp+esi-" + parm.substr(1,len-4) + "]";
        else
            out = "byte ptr [ebp+esi+" + parm.substr(0,len-3) + "]";
    }
    else if( util::suffix(parm,"(Y)" ))
    {
        if( parm[0] == '-' )
            out = "byte ptr [ebp+edi-" + parm.substr(1,len-4) + "]";
        else
            out = "byte ptr [ebp+edi+" + parm.substr(0,len-3) + "]";
    }
    else if( isascii(parm[0]) && isalpha(parm[0]) )
    {
        out = "byte ptr [ebp+"+ parm +"]";
    }
    else
    {
        ret = false;
    }
    return ret;
}

bool is_mem16( const std::string &parm, std::string &out )
{
    out = "word ptr [ebp+"+ parm +"]";
    return true;
}

bool is_imm16( const std::string &parm, std::string &out, const std::set<std::string> &labels )
{
    bool ret = true;
    out = parm; // don't actually check anything at least for now
    return ret;

/*  std::string temp = parm;
    size_t offset = temp.find_first_of("+-");
    if( offset != std::string::npos )
        temp = temp.substr(0,offset);
    auto it = labels.find(temp);
    if( it == labels.end() )
        out = parm;
    else
        out = "offset "+ parm;
    return true; */
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
        out = "byte ptr [ebp+ebx]";
    else
        ret = false;
    return ret;
}

bool is_reg16( const std::string &parm, std::string &out )
{
    bool ret = true;
    if( parm == "H" )
        out = "bx";
    else if( parm == "B" )
        out = "cx";
    else if( parm == "D" )
        out = "dx";
    else if( parm == "X" )
        out = "si";
    else if( parm == "Y" )
        out = "di";
    else if( parm == "SP" )
        out = "sp";
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

bool is_mem8_z80( const std::string &parm, std::string &out, bool hybrid )
{
    bool ret = true;
    int len = parm.length();
    if( util::suffix(parm,"(X)" ))
    {
        if( parm[0] == '-' )
            out = (hybrid?"(si-":"(ix-") + parm.substr(1,len-4) + ")";
        else
            out = (hybrid?"(si+":"(ix+") + parm.substr(0,len-3) + ")";
    }
    else if( util::suffix(parm,"(Y)" ))
    {
        if( parm[0] == '-' )
            out = (hybrid?"(di-":"(iy-") + parm.substr(1,len-4) + ")";
        else
            out = (hybrid?"(di+":"(iy+") + parm.substr(0,len-3) + ")";
    }
    else if( isascii(parm[0]) && isalpha(parm[0]) )
    {
        out = "("+ parm +")";
    }
    else
    {
        ret = false;
    }
    return ret;
}

bool is_mem16_z80( const std::string &parm, std::string &out )
{
    out = "("+ parm +")";
    return true;
}

bool is_imm16_z80( const std::string &parm, std::string &out )
{
    out = parm;
    return true;
}

bool is_reg8_z80( const std::string &parm, std::string &out, bool hybrid )
{
    bool ret = true;
    if( parm == "A" )
        out = hybrid ? "al" : "a";
    else if( parm == "B" )
        out = hybrid ? "ch" : "b";
    else if( parm == "C" )
        out = hybrid ? "cl" : "c";
    else if( parm == "D" )
        out = hybrid ? "dh" : "d";
    else if( parm == "E" )
        out = hybrid ? "dl" : "e";
    else if( parm == "H" )
        out = hybrid ? "bh" : "h";
    else if( parm == "L" )
        out = hybrid ? "bl" : "l";
    else if( parm == "M" )
        out = hybrid ? "(bx)" : "(hl)";
    else
        ret = false;
    return ret;
}

bool is_reg16_z80( const std::string &parm, std::string &out, bool hybrid )
{
    bool ret = true;
    if( parm == "H" )
        out = hybrid ? "bx" : "hl";
    else if( parm == "B" )
        out = hybrid ? "cx" : "bc";
    else if( parm == "D" )
        out = hybrid ? "dx" : "de";
    else if( parm == "X" )
        out = hybrid ? "si" : "ix";
    else if( parm == "Y" )
        out = hybrid ? "di" : "iy";
    else if( parm == "SP" )
        out = "sp";
    else
        ret = false;
    return ret;
}

bool is_reg8_mem8_z80( const std::string &parm, std::string &out, bool hybrid )
{
    bool ret = true;
    if( !is_reg8_z80(parm,out,hybrid) )
        ret = is_mem8_z80(parm,out,hybrid);
    return ret;
}

bool is_imm8_z80( const std::string &parm, std::string &out )
{
    bool ret = true;
    out = parm; // don't actually check anything at least for now
    return ret;
}


void xlat_registers_to_hybrid( std::string &s )
{
    util::replace_all( s, "a,", "al," );
    util::replace_all( s, "b,", "ch," );
    util::replace_all( s, "c,", "cl," );
    util::replace_all( s, "d,", "dh," );
    util::replace_all( s, "e,", "dl," );
    util::replace_all( s, "h,", "bh," );
    util::replace_all( s, "l,", "bl," );
}

// Return true if translated    
bool translate_z80( const std::string &line, const std::string &instruction, const std::vector<std::string> &parameters, bool hybrid, std::string &z80_out )
{
    z80_out = "";
    auto it1 = xlat.find(instruction.c_str());
    if( it1 == xlat.end() )
    {
        printf( "Error: Unknown instruction %s, line=[%s]\n", instruction.c_str(), line.c_str() );
        return false;
    }

    MnemonicConversion &mc = it1->second;
    const char *format = (hybrid && mc.hybrid) ? mc.hybrid : mc.z80;
    ParameterPattern pp = mc.pp;
    std::string parameters_listed;
    bool first = true;
    for( const std::string s: parameters )
    {
        parameters_listed += first ? "" : ",";
        first = false;
        parameters_listed += s;
    }
    if( pp!=none && pp!=jump_around && parameters.size()<1 )
    {
        printf( "Error: Expect at least one parameter, instruction=[%s], line=[%s]\n", instruction.c_str(), line.c_str() );
        return false;
    }
    switch( pp )
    {
        default:
        {
            printf( "Error: Unknown parameter pattern, instruction=[%s], line=[%s]\n", instruction.c_str(), line.c_str() );
            return false;
        }
        case jump_around:   // z80 doesn't need to jump around
        case none:
        {
            z80_out = std::string(format);
            break;
        }
        case jump_addr_around:   // z80 doesn't need to jump around
        case echo:
        {
            z80_out = util::sprintf( format, parameters_listed.c_str() );
            break;
        }
        case imm8:
        {
            std::string parm = parameters[0];
            std::string out;
            if( is_imm8_z80(parm,out) )
                z80_out = util::sprintf( format, out.c_str() );
            else
            {
                printf( "Error: Illegal imm8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case mem8:
        {
            std::string parm = parameters[0];
            std::string out;
            if( is_mem8_z80(parm,out,hybrid) )
                z80_out = util::sprintf( format, out.c_str() );
            else
            {
                printf( "Error: Illegal mem8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case mem16:
        {
            std::string parm = parameters[0];
            std::string out;
            if( is_mem16_z80(parm,out) )
                z80_out = util::sprintf( format, out.c_str() );
            else
            {
                printf( "Error: Illegal mem16 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case set_n_reg8_mem8:
        case clr_n_reg8:
        case set_n_reg8:
        {
            if( parameters.size() < 2 )
            {
                printf( "Error: Need two parameters, line=[%s]\n", line.c_str() );
                return false;
            }
            std::string parm = parameters[1];
            std::string out;
            if( is_reg8_mem8_z80(parm,out,hybrid) )
                z80_out = util::sprintf( format, parameters[0].c_str(), out.c_str() );
            else if( is_reg8_z80(parm,out,hybrid) )
                z80_out = util::sprintf( format, parameters[0].c_str(), out.c_str() );
            else
                printf( "Error: Illegal second parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
            break;
        }
        case push_parameter:
        {
            std::string parm = parameters[0];
            std::string out;
            if( parm == "PSW" )
                z80_out = "PUSH\taf";
            else if( is_reg16_z80(parm,out,hybrid) )
                z80_out = util::sprintf( "PUSH\t%s", out.c_str() );
            else
            {
                printf( "Error: Illegal push parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case pop_parameter:
        {
            std::string parm = parameters[0];
            std::string out;
            if( parm == "PSW" )
                z80_out = "POP\taf";
            else if( is_reg16_z80(parm,out,hybrid) )
                z80_out = util::sprintf( "POP\t%s", out.c_str() );
            else
            {
                printf( "Error: Illegal pop parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case reg8:
        {
            std::string parm = parameters[0];
            std::string out;
            if( is_reg8_z80(parm,out,hybrid) )
                z80_out = util::sprintf( format, out.c_str() );
            else
            {
                printf( "Error: Unknown reg8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case reg16:
        {
            std::string parm = parameters[0];
            std::string out;
            if( is_reg16_z80(parm,out,hybrid) )
                z80_out = util::sprintf( format, out.c_str() );
            else
            {
                printf( "Error: Unknown reg16 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case reg8_mem8:
        {
            std::string parm = parameters[0];
            std::string out;
            if( is_reg8_mem8_z80(parm,out,hybrid) )
                z80_out = util::sprintf( format, out.c_str() );
            else
            {
                printf( "Error: Unknown reg8_mem8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case reg8_mem8_reg8_mem8:
        {
            if( parameters.size() < 2 )
            {
                printf( "Error: Need two parameters, line=[%s]\n", line.c_str() );
                return false;
            }
            std::string parm = parameters[0];
            std::string out1;
            if( !is_reg8_mem8_z80(parm,out1,hybrid) )
            {
                printf( "Error: Unknown reg8_mem8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            parm = parameters[1];
            std::string out2;
            if( is_reg8_mem8_z80(parm,out2,hybrid) )
                z80_out = util::sprintf( format, out1.c_str(), out2.c_str() );
            else
            {
                printf( "Error: Unknown reg8_mem8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case reg8_mem8_imm8:
        {
            if( parameters.size() < 2 )
            {
                printf( "Error: Need two parameters, line=[%s]\n", line.c_str() );
                return false;
            }
            std::string parm = parameters[0];
            std::string out1;
            if( !is_reg8_mem8_z80(parm,out1,hybrid) )
            {
                printf( "Error: Unknown reg8_mem8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            parm = parameters[1];
            std::string out2;
            if( is_imm8_z80(parm,out2) )
                z80_out = util::sprintf( format, out1.c_str(), out2.c_str() );
            else
            {
                printf( "Error: Unknown imm8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case reg16_imm16:
        {
            if( parameters.size() < 2 )
            {
                printf( "Error: Need two parameters, line=[%s]\n", line.c_str() );
                return false;
            }
            std::string parm = parameters[0];
            std::string out1;
            if( !is_reg16_z80(parm,out1,hybrid) )
            {
                printf( "Error: Unknown reg16 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            parm = parameters[1];
            std::string out2;
            if( is_imm16_z80(parm,out2) )
                z80_out = util::sprintf( format, out1.c_str(), out2.c_str() );
            else
            {
                printf( "Error: Illegal imm16 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case rst_z80:
        {
            if( parameters.size() != 1 )
            {
                printf( "Error: Need one parameter, line=[%s]\n", line.c_str() );
                return false;
            }
            std::string parm = parameters[0];
            std::string out1;
            if( parm == "7" )
                out1 = "38h";
            else if( parm == "6" )
                out1 = "30h";
            else if( parm == "5" )
                out1 = "28h";
            else if( parm == "4" )
                out1 = "20h";
            else if( parm == "3" )
                out1 = "18h";
            else if( parm == "2" )
                out1 = "10h";
            else if( parm == "1" )
                out1 = "08h";
            else if( parm == "0" )
                out1 = "00h";
            else
            {
                printf( "Error: Illegal rst parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            z80_out = util::sprintf( format, out1.c_str() );
            break;
        }
    }
    return true;
}

// Return true if translated    
bool translate_x86( const std::string &line, const std::string &instruction, const std::vector<std::string> &parameters, std::set<std::string> &labels, std::string &x86_out )
{
    auto it1 = xlat.find(instruction.c_str());
    x86_out = "";
    if( it1 == xlat.end() )
    {
        printf( "Error: Unknown instruction %s, line=[%s]\n", instruction.c_str(), line.c_str() );
        return false;
    }
    MnemonicConversion &mc = it1->second;
    const char *format  = mc.x86;
    ParameterPattern pp = mc.pp;
    std::string parameters_listed;
    bool first = true;
    for( const std::string s: parameters )
    {
        parameters_listed += first ? "" : ",";
        first = false;
        parameters_listed += s;
    }
    if( pp!=none && pp!=jump_around && parameters.size()<1 )
    {
        printf( "Error: Expect at least one parameter, instruction=[%s], line=[%s]\n", instruction.c_str(), line.c_str() );
        return false;
    }
    switch( pp )
    {
        default:
        {
            printf( "Error: Unknown parameter pattern, instruction=[%s], line=[%s]\n", instruction.c_str(), line.c_str() );
            return false;
        }
        case none:
        {
            x86_out = std::string(format);
            break;
        }
        case echo:
        {
            x86_out = util::sprintf( format, parameters_listed.c_str() );
            break;
        }
        case imm8:
        {
            std::string parm = parameters[0];
            std::string out;
            if( is_imm8(parm,out) )
                x86_out = util::sprintf( format, out.c_str() );
            else
            {
                printf( "Error: Illegal imm8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case jump_addr_around:
        {
            std::string temp = util::sprintf( "skip%d", skip_counter++ );
            x86_out = util::sprintf( format, temp.c_str(), parameters[0].c_str(), temp.c_str() );
            break;
        }
        case jump_around:
        {
            std::string temp = util::sprintf( "skip%d", skip_counter++ );
            x86_out = util::sprintf( format, temp.c_str(), temp.c_str() );
            break;
        }
        case mem8:
        {
            std::string parm = parameters[0];
            std::string out;
            if( is_mem8(parm,out) )
                x86_out = util::sprintf( format, out.c_str() );
            else
            {
                printf( "Error: Illegal mem8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case mem16:
        {
            std::string parm = parameters[0];
            std::string out;
            if( is_mem16(parm,out) )
                x86_out = util::sprintf( format, out.c_str() );
            else
            {
                printf( "Error: Illegal mem16 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case set_n_reg8_mem8:
        {
            if( parameters.size() < 2 )
            {
                printf( "Error: Need two parameters, line=[%s]\n", line.c_str() );
                return false;
            }
            std::string parm = parameters[0];
            std::string out1;
            if( !is_n_set(parm,out1) )
            {
                printf( "Error: Unknown n parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            parm = parameters[1];
            std::string out2;
            if( is_reg8_mem8(parm,out2) )
                x86_out = util::sprintf( format, out2.c_str(), out1.c_str() );     // note reverse order
            else
            {
                printf( "Error: Illegal reg8_mem8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case set_n_reg8:
        {
            if( parameters.size() < 2 )
            {
                printf( "Error: Need two parameters, line=[%s]\n", line.c_str() );
                return false;
            }
            std::string parm = parameters[0];
            std::string out1;
            if( !is_n_set(parm,out1) )
            {
                printf( "Error: Unknown n parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            parm = parameters[1];
            std::string out2;
            if( is_reg8(parm,out2) )
                x86_out = util::sprintf( format, out2.c_str(), out1.c_str() );     // note reverse order
            else
            {
                printf( "Error: Illegal reg8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case clr_n_reg8:
        {
            if( parameters.size() < 2 )
            {
                printf( "Error: Need two parameters, line=[%s]\n", line.c_str() );
                return false;
            }
            std::string parm = parameters[0];
            std::string out1;
            if( !is_n_clr(parm,out1) )
            {
                printf( "Error: Unknown n parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            parm = parameters[1];
            std::string out2;
            if( is_reg8(parm,out2) )
                x86_out = util::sprintf( format, out2.c_str(), out1.c_str() );     // note reverse order
            else
            {
                printf( "Error: Illegal reg8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case push_parameter:
        {
            std::string parm = parameters[0];
            std::string out;
            if( parm == "PSW" )
                x86_out = "LAHF\n\tPUSH\teax";
            else if( is_reg16(parm,out) )
                x86_out = util::sprintf( "PUSH\te%s", out.c_str() );
            else
            {
                printf( "Error: Illegal push parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case pop_parameter:
        {
            std::string parm = parameters[0];
            std::string out;
            if( parm == "PSW" )
                x86_out = "POP\teax\n\tSAHF";
            else if( is_reg16(parm,out) )
                x86_out = util::sprintf( "POP\te%s", out.c_str() );
            else
            {
                printf( "Error: Illegal pop parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case reg8:
        {
            std::string parm = parameters[0];
            std::string out;
            if( is_reg8(parm,out) )
                x86_out = util::sprintf( format, out.c_str() );
            else
            {
                printf( "Error: Unknown reg8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case reg16:
        {
            std::string parm = parameters[0];
            std::string out;
            if( is_reg16(parm,out) )
            {
                std::string s(format);
                if( s.find_first_of("%") != s.find_last_of("%") )
                    x86_out = util::sprintf( format, out.c_str(), out.c_str() );
                else
                    x86_out = util::sprintf( format, out.c_str() );
            }
            else
            {
                printf( "Error: Unknown reg16 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case reg8_mem8:
        {
            std::string parm = parameters[0];
            std::string out;
            if( is_reg8_mem8(parm,out) )
                x86_out = util::sprintf( format, out.c_str() );
            else
            {
                printf( "Error: Unknown reg8_mem8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case reg8_mem8_reg8_mem8:
        {
            if( parameters.size() < 2 )
            {
                printf( "Error: Need two parameters, line=[%s]\n", line.c_str() );
                return false;
            }
            std::string parm = parameters[0];
            std::string out1;
            if( !is_reg8_mem8(parm,out1) )
            {
                printf( "Error: Unknown reg8_mem8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            parm = parameters[1];
            std::string out2;
            if( is_reg8_mem8(parm,out2) )
                x86_out = util::sprintf( format, out1.c_str(), out2.c_str() );
            else
            {
                printf( "Error: Unknown reg8_mem8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case reg8_mem8_imm8:
        {
            if( parameters.size() < 2 )
            {
                printf( "Error: Need two parameters, line=[%s]\n", line.c_str() );
                return false;
            }
            std::string parm = parameters[0];
            std::string out1;
            if( !is_reg8_mem8(parm,out1) )
            {
                printf( "Error: Unknown reg8_mem8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            parm = parameters[1];
            std::string out2;
            if( is_imm8(parm,out2) )
                x86_out = util::sprintf( format, out1.c_str(), out2.c_str() );
            else
            {
                printf( "Error: Unknown imm8 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case reg16_imm16:
        {
            if( parameters.size() < 2 )
            {
                printf( "Error: Need two parameters, line=[%s]\n", line.c_str() );
                return false;
            }
            std::string parm = parameters[0];
            std::string out1;
            if( !is_reg16(parm,out1) )
            {
                printf( "Error: Unknown reg16 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            parm = parameters[1];
            std::string out2;
            if( is_imm16(parm,out2,labels) )
                x86_out = util::sprintf( format, out1.c_str(), out2.c_str() );
            else
            {
                printf( "Error: Illegal imm16 parameter %s, line=[%s]\n", parm.c_str(), line.c_str() );
                return false;
            }
            break;
        }
        case rst_z80:
        {
            printf( "Error: RST doesn\'t translate to x86 line=[%s]\n", line.c_str() );
            return false;
        }
    }
    return true;
}



void translate_init()
{
    //
    // 8 bit register move
    //

    // MOV reg8_mem8, reg8_mem8 -> MOV reg8_mem8, reg8_mem8
    xlat["MOV"] = { "MOV\t%s,%s",  "LD\t%s,%s", NULL, reg8_mem8_reg8_mem8 };

    // MVI reg8_mem8, imm8 -> MOV reg8_mem8, imm8
    xlat["MVI"] = { "MOV\t%s,%s", "LD\t%s,%s", NULL, reg8_mem8_imm8 };

    //
    // Push and pop
    //

    // PUSH reg16 -> PUSH reg32
    // PUSH PSW -> LAHF; PUSH AX
    xlat["PUSH"] = { "%s", "%s", NULL, push_parameter };

    // POP reg16 -> POP reg32
    // POP PSW -> POP AX; SAHF
    xlat["POP"] = { "%s", "%s", NULL, pop_parameter };

    //
    // 8 bit arithmetic and logic
    //

    // ADD reg8_mem8 -> ADD al,reg8_mem8
    xlat["ADD"] = { "ADD\tal,%s",  "ADD\ta,%s",  "ADD\tal,%s", reg8_mem8 };

    // ADI imm8 -> ADD al,imm8
    xlat["ADI"] = { "ADD\tal,%s", "ADD\ta,%s", "ADD\tal,%s", imm8 };

    // ANA reg8_mem8 -> AND al,reg8_mem8
    xlat["ANA"] = { "AND\tal,%s", "AND\ta,%s", "AND\tal,%s", reg8_mem8 };

    // ANI imm8 -> AND al,imm8
    xlat["ANI"] = { "AND\tal,%s", "AND\ta,%s", "AND\tal,%s", imm8 };

    // SUB reg8_mem8 -> SUB al,reg8_mem8
    xlat["SUB"] = { "SUB\tal,%s",  "SUB\ta,%s", "SUB\tal,%s", reg8_mem8 };

    // SUI imm8 -> SUB al,imm8
    xlat["SUI"] = { "SUB\tal,%s", "SUB\ta,%s", "SUB\tal,%s", imm8 };

    // XRA reg8_mem8 -> XOR al,reg8_mem8
    xlat["XRA"] = { "XOR\tal,%s", "XOR\ta,%s", "XOR\tal,%s", reg8_mem8 };

    // XRI imm8 -> XOR al,imm8
    xlat["XRI"] = { "XOR\tal,%s", "XOR\ta,%s", "XOR\tal,%s", imm8 };

    // CMP reg8_mem8 -> CMP al,reg8_mem8
    xlat["CMP"] = { "CMP\tal,%s", "CP\ta,%s", "CP\tal,%s", reg8_mem8 };

    // CPI imm8 -> CMP al, imm8
    xlat["CPI"] = { "CMP\tal,%s", "CP\ta,%s", "CP\tal,%s", imm8 };

    // DCR reg8 -> DEC reg8
    xlat["DCR"] = { "DEC\t%s", "DEC\t%s", NULL, reg8 };

    // INR reg8 -> INC reg8
    xlat["INR"] = { "INC\t%s", "INC\t%s", NULL, reg8 };

    //
    // 16 bit arithmetic
    //

    // DAD reg16 -> LAHF; ADD ebx,reg16; SAHF        //CY (only) should be affected, but our emulation preserves flags
    xlat["DAD"] = { "LAHF\n\tADD\tbx,%s\n\tSAHF",  "ADD\thl,%s", "ADD\tbx,%s", reg16 };

    // DADX reg16 -> LAHF; ADD esi,reg16; SAHF       //CY (only) should be affected, but our emulation preserves flags
    xlat["DADX"] = { "LAHF\n\tADD\tsi,%s\n\tSAHF", "ADD\tix,%s", "ADD\tsi,%s", reg16 };

    // DADY reg16 -> LAHF; ADD edi,reg16; SAHF;      //CY (only) should be affected, but our emulation preserves flags
    xlat["DADY"] = { "LAHF\n\tADD\tdi,%s\n\tSAHF",  "ADD\tiy,%s", "ADD\tdi,%s", reg16 };  // not actually used in codebas

    // DSBC reg16 -> SBB ebx,reg16
    xlat["DSBC"] = { "SBB\tbx,%s",  "SBC\thl,%s",  "SBC\tbx,%s", reg16 };

    // INX reg16 -> LAHF; INC reg16; SAHF;      ## INC reg16; Z80 flags unaffected, X86 INC preserve CY only
    xlat["INX"] = { "LAHF\n\tINC\t%s\n\tSAHF",  "INC\t%s", NULL, reg16 };

    // DCX reg16 -> LAHF; DEC reg16; SAHF;      ## DEC reg16; Z80 flags unaffected, X86 DEC preserve CY only
    xlat["DCX"] = { "LAHF\n\tDEC\t%s\n\tSAHF", "DEC\t%s", NULL, reg16 };

    //
    // Bit test, set, clear
    //

    // BIT n,reg8_mem8 -> TEST reg8_mem8,mask(n) # but damages other flags
    xlat["BIT"] = { "TEST\t%s,%s", "BIT\t%s,%s", NULL, set_n_reg8_mem8 };

    // SET n,reg8 -> LAHF; OR reg8,mask[n]; SAHF
    xlat["SET"] = { "LAHF\n\tOR\t%s,%s\n\tSAHF", "SET\t%s,%s", NULL, set_n_reg8 };

    // RES n,reg8 -> LAHF; AND reg8,not mask[n]; SAHF
    xlat["RES"] = { "LAHF\n\tAND\t%s,%s\n\tSAHF", "RES\t%s,%s", NULL, clr_n_reg8 };

    //
    // Rotate and Shift
    //

    /*
      Rotate and shift reference info

              8080 (ext)               Z80                              X86
              ----                     ---                              ---
              RLC  (rotate A left)     RLCA                             ROL al,1
              RLCR reg (ext)           RLC reg *                        ROL reg,1
              RAL  (RL thru CY)        RLA                              RCL al,1
              RALR reg (ext)           RL reg *                         RCL reg,1
              RRC  (rotate A right)    RRCA                             ROR al,1
              RRCR reg (ext)           RRC reg  *                       ROR reg,1
              RAR  (RR thru CY)        RRA                              RCR al,1
              RARR reg (ext)           RR reg *                         RCR reg,1
              SRLR reg (ext)           SRL reg (shift right logical) ^  SHR reg,1  (shift right)
              SRAR reg (ext)           SRA reg (shift right arithmetic) SAR reg,1  (shift arithmetic right)
              SLAR reg (ext)           SLA reg (shift left arithmetic)  SAL reg,1  (or SHL reg,1)
              RLD (ext)                RLD (rotate BCD digit left)      - 
              RRD (ext)                RRD (rotate BCD digit right)     -

              The (ext) instructions here are not present at all in the actual
              8080, but these mnemonics are 8080 style extensions for the Z80
              in the TDL macro assembler used for the original Sargon source

              * RLC stands for "Rotate Left Circular", the CY bit is set to
                reflect the bit rotated from bit 7 to bit 0. Also RLC A (etc)
                is not quite a two byte equivalent to the one byte original
                8080 instruction set RLCA because it affects the flags differently

              ^ arithmetic right shifts copy bit 7, logic right shifts clear it
                arithmetic and logic left shifts are the same, clearing bit 0
                for X86 'shift' = shift logical (logical is assumed) and both
                forms of left shifts are allowed, as synonyms.

    */

    // Only implement the instructions that are present in the Sargon source code

    // RAL -> RCL al,1             # rotate left through CY
    xlat["RAL"] = { "RCL\tal,1",  "RLA", NULL, none };

    // RARR reg8 -> RCR reg, 1     # rotate right through CY
    xlat["RARR"] = { "RCR\t%s,1", "RR\t%s", NULL, reg8 };

    // RLD -> macro or call; 12 bits of low AL and byte [BX] rotated 4 bits left (!!)
    xlat["RLD"] = { "Z80_RLD", "RLD", NULL, none };

    // RRD -> macro or call; 12 bits of low AL and byte [BX] rotated 4 bits right (!!)
    xlat["RRD"] = { "Z80_RRD", "RRD", NULL, none };

    // SLAR reg8 -> SHL reg8,1     # left shift into CY, bit 0 zeroed (arithmetic and logical are the same)
    xlat["SLAR"] = { "SHL\t%s,1", "SLA\t%s", NULL, reg8 };

    // SRAR reg8 -> SAR reg8,1     # arithmetic right shift into CY, bit 7 preserved
    xlat["SRAR"] = { "SAR\t%s,1", "SRA\t%s", NULL, reg8 };

    // SRLR reg8 -> SHR reg8,1     # logical right shift into CY, bit 7 zeroed
    xlat["SRLR"] = { "SHR\t%s,1", "SRL\t%s", NULL, reg8 };

    //
    // Calls
    //

    /*

      Conditional branches reference info

      There is an excellent X86 Jump reference at www.unixwiz.net/techtips/x86-jumps.html
      that explains all the X86 synonyms. For our purposes only S=1, S=0 are potential
      hazards, and I did indeed handle these wrongly first time through

      The Z80 has both JP and JR instructions. The JR (R=relative) instructions are only
      two bytes. The JR instruction is restricted to unconditional or C,NC,Z and NZ
      conditionals only.

      The X86 does not allow conditional calls or returns, and the assembler determines
      whether to use short/long relative/absolute operands.

              8080                     Z80                        X86
              ----                     ---                        ---
     C=1      C  (JC, RC, CC)          C  (JP C, RET C, CALL C)   C  (JC)
     C=0      NC                       NC                         NC
     Z=1      Z                        Z                          Z
     Z=0      NZ                       NZ                         NZ
     S=1      M                        M                          S
     S=0      P                        P                          NS
     P=1      PE                       PE                         PE
     P=0      PO                       PO                         PO

    */


    // CALL addr -> CALL addr
    xlat["CALL"] = { "CALL\t%s", "CALL\t%s", NULL, echo };

    // CC addr -> JNC temp; CALL addr; temp:
    xlat["CC"] = { "JNC\t%s\n\tCALL\t%s\n%s:", "CALL\tC,%s", NULL, jump_addr_around };

    // CNZ addr -> JZ temp; CALL addr; temp:
    xlat["CNZ"] = { "JZ\t%s\n\tCALL\t%s\n%s:", "CALL\tNZ,%s", NULL, jump_addr_around };

    // CZ addr -> JNZ temp; CALL addr; temp:
    xlat["CZ"] = { "JNZ\t%s\n\tCALL\t%s\n%s:", "CALL\tZ,%s", NULL, jump_addr_around };

    //
    // Returns
    //

    // RET -> RET
    xlat["RET"] = { "RET", "RET", NULL, none };

    // RC -> JNC temp; RET; temp:
    xlat["RC"] = { "JNC\t%s\n\tRET\n%s:", "RET\tC", NULL, jump_around };

    // RNC -> JC temp; RET; temp:
    xlat["RNC"] = { "JC\t%s\n\tRET\n%s:", "RET\tNC", NULL, jump_around };

    // RNZ -> JZ temp; RET; temp:
    xlat["RNZ"] = { "JZ\t%s\n\tRET\n%s:", "RET\tNZ", NULL, jump_around };

    // RZ -> JNZ temp; RET; temp:
    xlat["RZ"] = { "JNZ\t%s\n\tRET\n%s:", "RET\tZ", NULL, jump_around };

    // RM -> JNS temp; RET; temp:
    xlat["RM"] = { "JNS\t%s\n\tRET\n%s:", "RET\tM", NULL, jump_around };

    // RP -> JS temp; RET; temp:
    xlat["RP"] = { "JS\t%s\n\tRET\n%s:", "RET\tP", NULL, jump_around };

    //
    // Jumps
    //


    // DJNZ addr -> LAHF; DEC ch; JNZ addr; SAHF; ## flags affected at addr (sadly not much to be done)
    xlat["DJNZ"] = { "LAHF\n\tDEC\tch\n\tJNZ\t%s\n\tSAHF", "DJNZ\t%s", NULL, echo };

    // JC addr -> JC addr
    xlat["JC"] = { "JC\t%s", "JP\tC,%s", NULL, echo };

    // JM addr -> JS addr  (jump if sign bit true = most sig bit true = negative)
    xlat["JM"] = { "JS\t%s", "JP\tM,%s", NULL, echo };

    // JMP addr -> JMP addr
    xlat["JMP"] = { "JMP\t%s", "JP\t%s", NULL, echo };

    // JMPR addr -> JMP addr (avoid relative jumps unless we get into mega optimisation)
    xlat["JMPR"] = { "JMP\t%s", "JR\t%s", NULL, echo };

    // JC addr -> JC addr
    xlat["JC"] = { "JC\t%s", "JP\tC,%s", NULL, echo };

    // JNC addr -> JNC addr
    xlat["JNC"] = { "JNC\t%s", "JP\tNC,%s", NULL, echo };

    // JZ addr -> JZ addr
    xlat["JZ"] = { "JZ\t%s", "JP\tZ,%s", NULL, echo };

    // JNZ addr -> JNZ addr
    xlat["JNZ"] = { "JNZ\t%s", "JP\tNZ,%s", NULL, echo };

    // JP addr -> JNS addr  (jump sign not set [i.e. positive])
    xlat["JP"] = { "JNS\t%s", "JP\tP,%s", NULL, echo };

    // JM addr -> JS addr  (jump sign set [i.e. negative])
    xlat["JM"] = { "JS\t%s", "JP\tM,%s", NULL, echo };

    // JPE addr -> JPE addr  (jump parity even - check this one, parity bit not 100% compatible) 
    xlat["JPE"] = { "JPE\t%s",  "JP\tPE,%s", NULL, echo };

    // JPO addr -> JPO addr  (jump parity odd - doesn't occur in Sargon)
    xlat["JPO"] = { "JPO\t%s",  "JP\tPO,%s", NULL, echo };

    // JRC addr -> JC addr
    xlat["JRC"] = { "JC\t%s", "JR\tC,%s", NULL, echo };

    // JRNC addr -> JNC addr
    xlat["JRNC"] = { "JNC\t%s", "JR\tNC,%s", NULL, echo };

    // JRNZ addr -> JNZ addr
    xlat["JRNZ"] = { "JNZ\t%s", "JR\tNZ,%s", NULL, echo };

    // JRZ addr -> JZ addr
    xlat["JRZ"] = { "JZ\t%s", "JR\tZ,%s", NULL, echo };

    //
    // Load memory -> register
    //

    // LDA mem8 -> MOV AL,mem8
    xlat["LDA"] = { "MOV\tal,%s", "LD\ta,%s", "LD\tal,%s", mem8 };

    // LDAX reg16 -> MOV al,[reg16]
    xlat["LDAX"] = { "MOV\tal,byte ptr [ebp+e%s]", "LD\ta,(%s)", "LD\tal,(%s)", reg16 };

    // LHLD mem16 -> MOV ebx,[mem16]
    xlat["LHLD"] = { "MOV\tbx,%s", "LD\thl,%s", "LD\tbx,%s", mem16 };

    // LBCD mem16 -> MOV ecx,[mem16]
    xlat["LBCD"] = { "MOV\tcx,%s",  "LD\tbc,%s", "LD\tcx,%s", mem16 };

    // LDED mem16 -> MOV edx,[mem16]
    xlat["LDED"] = { "MOV\tdx,%s", "LD\tde,%s", "LD\tdx,%s", mem16 };

    // LIXD mem16 -> MOV esi,[mem16]
    xlat["LIXD"] = { "MOV\tsi,%s", "LD\tix,%s", "LD\tsi,%s", mem16 };

    // LIYD mem16 -> MOV edi,[mem16]
    xlat["LIYD"] = { "MOV\tdi,%s", "LD\tiy,%s", "LD\tdi,%s", mem16 };

    // LXI reg16, imm16 -> MOV reg16, imm16 (if imm16 is a label precede it with offset)
    xlat["LXI"] = { "MOV\t%s,%s", "LD\t%s,%s", NULL, reg16_imm16 };

    //
    // Store register -> memory
    //

    // STA mem8 -> MOV [mem8],al
    xlat["STA"]  = { "MOV\t%s,al", "LD\t%s,a", "LD\t%s,al", mem8 };

    // SHLD mem16 -> mov [mem16],ebx
    xlat["SHLD"] = { "MOV\t%s,bx", "LD\t%s,hl",  "LD\t%s,bx", mem16 };

    // SBCD mem16 -> MOV [mem16],ecx
    xlat["SBCD"] = { "MOV\t%s,cx", "LD\t%s,bc", "LD\t%s,cx", mem16 };

    // SDED mem16 -> mov [mem16],edx
    xlat["SDED"] = { "MOV\t%s,dx", "LD\t%s,de", "LD\t%s,dx", mem16 };

    // SIXD mem16 -> mov [mem16],esi
    xlat["SIXD"] = { "MOV\t%s,si", "LD\t%s,ix", "LD\t%s,si", mem16 };

    //
    // Miscellaneous
    //

    // NEG -> NEG al
    xlat["NEG"] = { "NEG\tal",  "NEG", NULL, none };

    // XCHG -> XCHG ebx,edx
    xlat["XCHG"] = { "XCHG\tbx,dx", "EX\tde,hl", "EX\tdx,bx", none };

    // EXAF -> macro/call
    xlat["EXAF"] = { "Z80_EXAF", "EX\taf,af'", NULL, none };

    // EXX -> macro/call
    xlat["EXX"] = { "Z80_EXX", "EXX", NULL, none };

    // LDAR -> Load A with incrementing R (RAM refresh) register (to get a random number)
    xlat["LDAR"] = { "Z80_LDAR", "LD\ta,r", NULL, none };

    // CPIR (CCIR in quirky assembler) -> macro/call
    xlat["CCIR"] = { "Z80_CPIR", "CPIR", NULL, none };

    // RST
    xlat["RST"] = { "Z80_RST",  "RST\t%s", "RST\t%s", rst_z80 };

    //
    // Macros
    //
    xlat["CARRET"] = { "CARRET", "CARRET", NULL, none };
    xlat["PRTBLK"] = { "PRTBLK\t%s", "PRTBLK\t%s", NULL, echo };
    //xlat["CALLBACK"] = { "CALLBACK\t%s", "CALLBACK\t%s", NULL, echo };

    //
    // Directives
    //
    xlat[".BLKB"] = { "DB\t%s DUP (?)", "DS\t%s", NULL, echo };
    xlat[".BLKW"] = { "DW\t%s DUP (?)", "DS\t2*(%s)", NULL, echo };
    xlat[".BYTE"] = { "DB\t%s", "DB\t%s", NULL, echo };
    xlat[".WORD"] = { "DD\t%s", "DW\t%s", NULL, echo };
    xlat[".LOC"]  = { ";ORG\t%s", "ORG\t%s", NULL, echo };
 }

 // Optionally replace the LAHF/SAHF versions - don't really need to preserve flags in these
 //  instructions in Sargon
 void translate_init_slim_down()
 {
    // DAD reg16 -> LAHF; ADD ebx,reg16; SAHF        //CY (only) should be affected, but our emulation preserves flags
    xlat["DAD"] = { "ADD\tbx,%s",  "ADD\thl,%s", "ADD\tbx,%s", reg16 };

    // DADX reg16 -> LAHF; ADD esi,reg16; SAHF       //CY (only) should be affected, but our emulation preserves flags
    xlat["DADX"] = { "ADD\tsi,%s", "ADD\tix,%s", "ADD\tsi,%s", reg16 };

    // DADY reg16 -> LAHF; ADD edi,reg16; SAHF;      //CY (only) should be affected, but our emulation preserves flags
    xlat["DADY"] = { "tADD\tdi,%s",  "ADD\tiy,%s", "ADD\tdi,%s", reg16 };  // not actually used in codebas

    // INX reg16 -> LAHF; INC reg16; SAHF;      ## INC reg16; Z80 flags unaffected, X86 INC preserve CY only
    xlat["INX"] = { "INC\t%s",  "INC\t%s", NULL, reg16 };

    // DCX reg16 -> LAHF; DEC reg16; SAHF;      ## DEC reg16; Z80 flags unaffected, X86 DEC preserve CY only
    xlat["DCX"] = { "DEC\t%s", "DEC\t%s", NULL, reg16 };

    // SET n,reg8 -> LAHF; OR reg8,mask[n]; SAHF
    xlat["SET"] = { "OR\t%s,%s", "SET\t%s,%s", NULL, set_n_reg8 };

    // RES n,reg8 -> LAHF; AND reg8,not mask[n]; SAHF
    xlat["RES"] = { "AND\t%s,%s", "RES\t%s,%s", NULL, clr_n_reg8 };

    // DJNZ addr -> LAHF; DEC ch; JNZ addr; SAHF; ## flags affected at addr (sadly not much to be done)
    xlat["DJNZ"] = { "DEC ch\n\tJNZ\t%s", "DJNZ\t%s", NULL, echo };
}

