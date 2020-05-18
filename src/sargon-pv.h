/****************************************************************************
 * This project is a Windows port of the classic program Sargon, as
 * presented in the book "Sargon a Z80 Computer Chess Program" by Dan
 * and Kathe Spracklen (Hayden Books 1978).
 *
 * File: sargon-pv.h
 *       Sargon PV (Principal Variation) Calculation
 *
 * Bill Forster, https://github.com/billforsternz/retro-sargon
 ****************************************************************************/

#ifndef SARGON_PV_H_INCLUDED
#define SARGON_PV_H_INCLUDED
  
#include <string>
#include "thc.h"

// PV
struct PV
{
    std::vector<thc::Move> variation;
    int value;
    int depth;
    void clear() {variation.clear(),value=0,depth=0;}
    PV () {clear();}
};

void sargon_pv_clear( const thc::ChessPosition &current_position );
PV sargon_pv_get();
void sargon_pv_callback_end_of_points();
void sargon_pv_callback_yes_best_move();
std::string sargon_pv_report_stats();

#endif // SARGON_PV_H_INCLUDED
