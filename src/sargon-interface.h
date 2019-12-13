/*

  Primitive Sargon interfacing functions
  
*/

#ifndef SARGON_INTERFACE_H_INCLUDED
#define SARGON_INTERFACE_H_INCLUDED
  
#include <string>
#include "thc.h"

// Read chess position from Sargon
void sargon_position_export( thc::ChessPosition &cp );

// Write chess position into Sargon
void sargon_position_import( const thc::ChessPosition &cp );

// Sargon value convention to and from centipawns
double sargon_value_export( unsigned int value );
unsigned int sargon_value_import( double value );

// Sargon square convention -> string
std::string algebraic( unsigned int sq );

// Peek and poke at Sargon
const unsigned char *peek(int offset);
unsigned char peekb(int offset);
unsigned int peekw(int offset);
unsigned char *poke(int offset);
void pokeb( int offset, unsigned char b );

#endif // SARGON_INTERFACE_H_INCLUDED
