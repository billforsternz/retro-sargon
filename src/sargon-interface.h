/****************************************************************************
 * This project is a Windows port of the classic program Sargon, as
 * presented in the book "Sargon a Z80 Computer Chess Program" by Dan
 * and Kathe Spracklen (Hayden Books 1978).
 *
 * File: sargon-interface.h
 *       Interfacing functions to the Sargon X86 assembler module
 *
 * Bill Forster, https://github.com/billforsternz/retro-sargon
 ****************************************************************************/

#ifndef SARGON_INTERFACE_H_INCLUDED
#define SARGON_INTERFACE_H_INCLUDED
  
#include <string>
#include "sargon-pv.h"
#include "thc.h"

// Read a square value out of Sargon
bool sargon_export_square( unsigned int sargon_square, thc::Square &sq );

// Read a chess move out of Sargon (returns "Terse" form - eg "e1g1" for White O-O, note
//  that Sargon always promotes to Queen, so four character form is sufficient)
std::string sargon_export_move( unsigned int sargon_move_ptr );

// Play a move inside Sargon (i.e. update Sargon's representation with a legal
//  played move)
bool sargon_play_move( thc::Move &mv );

// Read chess position from Sargon
void sargon_export_position( thc::ChessPosition &cp );

// Write chess position into Sargon
void sargon_import_position( const thc::ChessPosition &cp, bool avoid_book=false );

// Sargon value convention to and from centipawns
double sargon_export_value( unsigned int value );
unsigned int sargon_import_value( double value );

// Sargon square convention -> string
std::string algebraic( unsigned int sq );

// Run Sargon move calculation
void sargon_run_engine( const thc::ChessPosition &cp, int plymax, PV &pv, bool avoid_book );

// Peek and poke at Sargon
const unsigned char *peek(int offset);
unsigned char peekb(int offset);
unsigned int peekw(int offset);
unsigned char *poke(int offset);
void pokeb( int offset, unsigned char b );

#endif // SARGON_INTERFACE_H_INCLUDED
