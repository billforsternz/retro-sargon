/*

  Translate TDL Z80 Macro assembler to normal Z80 mnemonics and to X86 Assembler
  
*/

#ifndef TRANSLATE_H_INCLUDED
#define TRANSLATE_H_INCLUDED

void translate_init();
void translate_init_slim_down();

// Return true if translated    
bool translate_z80( const std::string &line, const std::string &instruction, const std::vector<std::string> &parameters, bool hybrid, std::string &out );

// Return true if translated    
bool translate_x86( const std::string &line, const std::string &instruction, const std::vector<std::string> &parameters, std::set<std::string> &labels, std::string &out );

#endif //TRANSLATE_H_INCLUDED
