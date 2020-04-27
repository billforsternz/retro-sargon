/*

  Sargon PV (Principal Variation) Calculation
  
*/

#ifndef SARGON_PV_H_INCLUDED
#define SARGON_PV_H_INCLUDED
  
#include <string>
#include "thc.h"

// PV
// A move in Sargon's evaluation graph, in this program a move that is marked as
//  the best move found so far at a given level
struct NODE
{
    unsigned int level;
    unsigned char from;
    unsigned char to;
    unsigned char flags;
    unsigned char value;
    NODE() : level(0), from(0), to(0), flags(0), value(0) {}
    NODE( unsigned int l, unsigned char f, unsigned char t, unsigned char fs, unsigned char v ) : level(l), from(f), to(t), flags(fs), value(v) {}
};

// A principal variation
struct PV
{
    std::vector<thc::Move> variation;
    int value;
    int depth;
    void clear() {variation.clear(),value=0,depth=0;}
    PV () {clear();}
};

void sargon_pv_clear( thc::ChessRules &current_position );
PV sargon_pv_get();
void sargon_pv_callback_yes_best_move();
std::string sargon_pv_report_stats();

#endif // SARGON_PV_H_INCLUDED
