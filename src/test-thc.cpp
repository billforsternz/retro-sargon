/*
 *  This is a simple test program to demonstrate how to use the simple chess library THC.
 *  It also serves as a simple 'hello world' type starter program, first get this going
 *  and then move on to more elaborate projects.
 *
 *  (THC stands for Triplehappy Chess - Triplehappy was a games company that I started
 *   with Scott Crabtree but which never really developed. I reused the name Triplehappy
 *   for my later chess projects, including the Tarrash Chess GUI which uses THC)
 *
 *  Bill Forster 2020.05.17
 *
 */

#include <stdio.h>
#include <string>
#include <vector>
#include "thc.h"

void display_position( thc::ChessRules &cr, const std::string &description )
{
    std::string fen = cr.ForsythPublish();
    std::string s = cr.ToDebugStr();
    printf( "%s\n", description.c_str() );
    printf( "FEN (Forsyth Edwards Notation) = %s\n", fen.c_str() );
    printf( "Position = %s\n", s.c_str() );
}

int main()
{
    // Example 1, Play a few good moves from the initial position
    thc::ChessRules cr;
    display_position( cr, "Initial position" );
    thc::Move mv;
    mv.NaturalIn( &cr, "e4" );
    cr.PlayMove(mv);
    mv.NaturalIn( &cr, "e5" );
    cr.PlayMove(mv);
    mv.NaturalIn( &cr, "Nf3" );
    cr.PlayMove(mv);
    mv.NaturalIn( &cr, "Nc6" );
    cr.PlayMove(mv);
    mv.NaturalIn( &cr, "Bc4" );
    cr.PlayMove(mv);
    mv.NaturalIn( &cr, "Bc5" );
    cr.PlayMove(mv);
    display_position( cr, "Starting position of Italian opening, after 1.e4 e5 2.Nf3 Nc6 3.Bc4 Bc5" );

    printf( "List of all legal moves in the current position\n" );
    std::vector<thc::Move> moves;
    std::vector<bool> check;
    std::vector<bool> mate;
    std::vector<bool> stalemate;
    cr.GenLegalMoveList(  moves, check, mate, stalemate );
    unsigned int len = moves.size();
    for( unsigned int i=0; i<len; i++ )
    {
        mv = moves[i];
        std::string mv_txt = mv.NaturalOut(&cr);
        const char *suffix="";
        if( check[i] )
            suffix = " (note '+' indicates check)";
        else if( mate[i] )
            suffix = " (note '#' indicates mate)";
        printf( "4.%s%s\n", mv_txt.c_str(), suffix );
    }

    // Example 2, The shortest game leading to mate
    printf("\n");
    thc::ChessRules cr2;
    cr = cr2;
    display_position( cr, "Initial position" );
    mv.TerseIn( &cr, "g2g4" );
    std::string description = "Position after 1." + mv.NaturalOut(&cr);
    cr.PlayMove(mv);
    display_position( cr, description );
    mv.TerseIn( &cr, "e7e5" );
    description = "Position after 1..." + mv.NaturalOut(&cr);
    cr.PlayMove(mv);
    display_position( cr, description );
    mv.TerseIn( &cr, "f2f4" );
    description = "Position after 2." + mv.NaturalOut(&cr);
    cr.PlayMove(mv);
    thc::TERMINAL eval_penultimate_position;
    bool legal1 = cr.Evaluate( eval_penultimate_position );
    bool normal1 = (eval_penultimate_position == thc::NOT_TERMINAL);
    display_position( cr, description );
    mv.TerseIn( &cr, "d8h4" );
    description = "Position after 2..." + mv.NaturalOut(&cr);
    cr.PlayMove(mv);
    display_position( cr, description );
    thc::TERMINAL eval_final_position;
    bool legal2 = cr.Evaluate( eval_final_position );
    bool mate2 = (eval_final_position == thc::TERMINAL_WCHECKMATE);
    printf( "legal1=%s, normal1=%s, legal2=%s, mate2=%s\n", 
        legal1?"true":"false",
        normal1?"true":"false",
        legal2?"true":"false",
        mate2?"true":"false");
    if( legal1 && normal1 && legal2 && mate2 )
        printf( "As expected, all flags true, so both penultimate and final positions are legal, in the final position White is mated\n" );
    else
        printf( "Strange(???!), we expected all flags true, meaning both penultimate and final positions are legal, in the final position White is mated\n" );
}

/*  Expected output of the program follows;

Initial position
FEN (Forsyth Edwards Notation) = rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
Position =
White to move
rnbqkbnr
pppppppp
........
........
........
........
PPPPPPPP
RNBQKBNR

Starting position of Italian opening, after 1.e4 e5 2.Nf3 Nc6 3.Bc4 Bc5
FEN (Forsyth Edwards Notation) = r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4
Position =
White to move
r.bqk.nr
pppp.ppp
..n.....
..b.p...
..B.P...
.....N..
PPPP.PPP
RNBQK..R

List of all legal moves in the current position
4.Bb3
4.Bb5
4.Ba6
4.Bd5
4.Be6
4.Bxf7+ (note '+' indicates check)
4.Bd3
4.Be2
4.Bf1
4.Nd4
4.Nxe5
4.Nh4
4.Ng1
4.Ng5
4.a3
4.a4
4.b3
4.b4
4.c3
4.d3
4.d4
4.g3
4.g4
4.h3
4.h4
4.Na3
4.Nc3
4.Qe2
4.Kf1
4.Ke2
4.O-O
4.Rg1
4.Rf1

Initial position
FEN (Forsyth Edwards Notation) = rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
Position =
White to move
rnbqkbnr
pppppppp
........
........
........
........
PPPPPPPP
RNBQKBNR

Position after 1.g4
FEN (Forsyth Edwards Notation) = rnbqkbnr/pppppppp/8/8/6P1/8/PPPPPP1P/RNBQKBNR b KQkq g3 0 1
Position =
Black to move
rnbqkbnr
pppppppp
........
........
......P.
........
PPPPPP.P
RNBQKBNR

Position after 1...e5
FEN (Forsyth Edwards Notation) = rnbqkbnr/pppp1ppp/8/4p3/6P1/8/PPPPPP1P/RNBQKBNR w KQkq e6 0 2
Position =
White to move
rnbqkbnr
pppp.ppp
........
....p...
......P.
........
PPPPPP.P
RNBQKBNR

Position after 2.f4
FEN (Forsyth Edwards Notation) = rnbqkbnr/pppp1ppp/8/4p3/5PP1/8/PPPPP2P/RNBQKBNR b KQkq f3 0 2
Position =
Black to move
rnbqkbnr
pppp.ppp
........
....p...
.....PP.
........
PPPPP..P
RNBQKBNR

Position after 2...Qh4#
FEN (Forsyth Edwards Notation) = rnb1kbnr/pppp1ppp/8/4p3/5PPq/8/PPPPP2P/RNBQKBNR w KQkq - 1 3
Position =
White to move
rnb.kbnr
pppp.ppp
........
....p...
.....PPq
........
PPPPP..P
RNBQKBNR

legal1=true, normal1=true, legal2=true, mate2=true
As expected, all flags true, so both penultimate and final positions are legal, in the final position White is mate

*/
