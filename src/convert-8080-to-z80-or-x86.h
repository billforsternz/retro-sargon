/****************************************************************************
 * This project is a Windows port of the classic program Sargon, as
 * presented in the book "Sargon a Z80 Computer Chess Program" by Dan
 * and Kathe Spracklen (Hayden Books 1978).
 *
 * File: convert-8080-to-z80-or-x86.h
 *       Translate TDL Z80 Macro assembler to normal Z80 mnemonics or to X86 Assembler
 *
 * Bill Forster, https://github.com/billforsternz/retro-sargon
 ****************************************************************************/

#ifndef TRANSLATE_H_INCLUDED
#define TRANSLATE_H_INCLUDED

// Initialise module
void translate_init();
void translate_init_slim_down();   // specify slimmed down code without LAHF/SAF guards

// Return true if translated    
bool translate_z80( const std::string &line, const std::string &instruction, const std::vector<std::string> &parameters, bool hybrid, std::string &out );

// Return true if translated    
bool translate_x86( const std::string &line, const std::string &instruction, const std::vector<std::string> &parameters, std::set<std::string> &labels, std::string &out );

#endif //TRANSLATE_H_INCLUDED
