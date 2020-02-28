/*

  This program tests a Windows port of the classic program Sargon, as
  presented in the book "Sargon a Z80 Computer Chess Program" by Dan and
  Kathe Spracklen (Hayden Books 1978). Another program in this suite converts
  the Z80 code to working X86 assembly language. A third program wraps the
  Sargon X86 code in a simple standard Windows UCI engine interface.
  
  */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include "util.h"
#include "thc.h"
#include "sargon-asm-interface.h"
#include "sargon-interface.h"

// Individual tests
bool sargon_position_tests( bool quiet, bool no_very_slow_tests );
bool sargon_whole_game_tests( bool quiet, bool no_very_slow_tests );
bool sargon_algorithm_explore( bool quiet, bool alpha_beta );

// Suites of tests
bool sargon_tests_quiet();
bool sargon_tests_quiet_comprehensive();
bool sargon_tests_verbose();
bool sargon_tests_verbose_comprehensive();

// Misc
static std::string convert_to_key( unsigned char from, unsigned char to, const thc::ChessPosition &cp );

// Misc diagnostics
void dbg_ptrs();
void dbg_position();
void diagnostics();
void on_exit_diagnostics() {}

// Nodes to track the PV (principal [primary?] variation)
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
static std::vector< NODE > nodes;

// Control callback() behaviour
static bool callback_enabled;
static bool callback_kingmove_suppressed;
static bool callback_quiet;
static std::vector<unsigned int> cardinal_list;

// Prime the callback routine to modify node values for simple 15 node minimax
//  algorithm example
void probe_test_prime( bool alpha_beta );

// main()
int main( int argc, const char *argv[] )
{
    util::tests();
    bool ok = sargon_tests_quiet_comprehensive();
    if( ok )
        printf( "All tests passed\n" );
    else
        printf( "Not all tests passed\n" );
    on_exit_diagnostics();
    return ok ? 0 : -1;
}

bool sargon_tests_quiet()
{
    bool ok=true;
    bool passed = sargon_algorithm_explore(true, false);
    if( !passed )
        ok = false;
    passed = sargon_algorithm_explore(true, true);
    if( !passed )
        ok = false;
    passed = sargon_position_tests(true, true);
    if( !passed )
        ok = false;
    passed = sargon_whole_game_tests(true, true);
    if( !passed )
        ok = false;
    return ok;
}

bool sargon_tests_quiet_comprehensive()
{
    bool ok=true;
    bool passed = sargon_algorithm_explore(true, false);
    if( !passed )
        ok = false;
    passed = sargon_algorithm_explore(true, true);
    if( !passed )
        ok = false;
    passed = sargon_position_tests(true, false);
    if( !passed )
        ok = false;
    passed = sargon_whole_game_tests(true, false);
    if( !passed )
        ok = false;
    return ok;
}

bool sargon_tests_verbose()
{
    bool ok=true;
    bool passed = sargon_algorithm_explore(false, false);
    if( !passed )
        ok = false;
    passed = sargon_algorithm_explore(false, true);
    if( !passed )
        ok = false;
    passed = sargon_position_tests(false, true);
    if( !passed )
        ok = false;
    passed = sargon_whole_game_tests(false, true);
    if( !passed )
        ok = false;
    return ok;
}

bool sargon_tests_verbose_comprehensive()
{
    bool ok=true;
    bool passed = sargon_algorithm_explore(false, false);
    if( !passed )
        ok = false;
    passed = sargon_algorithm_explore(false, true);
    if( !passed )
        ok = false;
    passed = sargon_position_tests(false, false);
    if( !passed )
        ok = false;
    passed = sargon_whole_game_tests(false, false);
    if( !passed )
        ok = false;
    return ok;
}

// Calculate a short string key to represent the moves played
//  eg 1. a4-a5 h6-h5 2. a5-a6 => "AHA"
static std::string convert_to_key( unsigned char from, unsigned char to, const thc::ChessPosition &cp )
{
    std::string key = "??";
    thc::Square sq_from, sq_to;
    int nmoves = 0;
    bool ok_from = sargon_export_square(from,sq_from);
    bool ok_to = sargon_export_square(to,sq_to);
    if( !ok_from || !ok_to )
        key = "(root)";
    else
    {
        bool a0 = (cp.squares[thc::a3] == 'P');
        bool a1 = (cp.squares[thc::a4] == 'P');
        bool a2 = (cp.squares[thc::a5] == 'P');
        if( a1 )
            nmoves += 1;
        else if( a2 )
            nmoves += 2;
        bool b0 = (cp.squares[thc::b3] == 'P');
        bool b1 = (cp.squares[thc::b4] == 'P');
        bool b2 = (cp.squares[thc::b5] == 'P');
        if( b1 )
            nmoves += 1;
        else if( b2 )
            nmoves += 2;
        bool g0 = (cp.squares[thc::g6] == 'p');
        bool g1 = (cp.squares[thc::g5] == 'p');
        if( g1 )
            nmoves += 1;
        bool h0 = (cp.squares[thc::h6] == 'p');
        bool h1 = (cp.squares[thc::h5] == 'p');
        if( h1 )
            nmoves += 1;
        bool a_last = (thc::get_file(sq_to) == 'a');
        bool b_last = (thc::get_file(sq_to) == 'b');
        bool g_last = (thc::get_file(sq_to) == 'g');
        bool h_last = (thc::get_file(sq_to) == 'h');
        if( nmoves == 3 )
        {
            if( a_last && a2 && g1)
                key = "AGA";
            else if( a_last && a1 && g1 )
                key = "BGA";
            else if( a_last && a2 && h1 )
                key = "AHA";
            else if( a_last && a1 && h1 )
                key = "BHA";
            else if( b_last && b2 && g1 )
                key = "BGB";
            else if( b_last && b1 && g1 )
                key = "AGB";
            else if( b_last && b2 && h1 )
                key = "BHB";
            else if( b_last && b1 && h1 )
                key = "AHB";
        }
        else if( nmoves == 2 )
        {
            if( g_last && a1 )
                key = "AG";
            else if( g_last && b1 )
                key = "BG";
            else if( h_last && a1 )
                key = "AH";
            else if( h_last && b1 )
                key = "BH";
        }
        else if( nmoves == 1 )
        {
            if( a1 )
                key = "A";
            else if( b1 )
                key = "B";
        }
    }
    return key;
}

// Use a simple example to explore/probe the minimax algorithm and verify it
bool sargon_algorithm_explore( bool quiet, bool alpha_beta  )
{
    bool ok=true;
    printf( "* Minimax %salgorithm tests\n", alpha_beta?"plus alpha-beta pruning ":"" );

    // W king on a1 pawns a3 and b3, B king on h8 pawns g6 and h6 we are going
    //  to use this very dumb position to probe Alpha Beta pruning etc. (we
    //  will kill the kings so that each side has only two moves available
    //  at each position)
    const char *pos_probe = "7k/8/6pp/8/8/PP6/8/K7 w - - 0 1";

    // Because there are only 2 moves available at each ply, we can explore
    //  to PLYMAX=3 with only 2 positions at ply 1, 4 positions at ply 2
    //  and 8 positions at ply 3 (plus 1 root position at ply 0) for a very
    //  manageable 1+2+4+8 = 15 nodes (i.e. positions) total. We use the
    //  callback facility to monitor the algorithm and indeed actively
    //  interfere with it by changing the node evals and watching how that
    //  effects node traversal and generates a best move.
    // Note that if alpha_beta=true the resulting node values will result
    // in alpha-beta pruning and all 15 nodes won't be traversed
    probe_test_prime(alpha_beta);
    callback_enabled = true;
    callback_kingmove_suppressed = true;
    callback_quiet = quiet;
    cardinal_list.clear();
    thc::ChessPosition cp;
    cp.Forsyth(pos_probe);
    pokeb(MLPTRJ,0); //need to set this ptr to 0 to get Root position recognised in callback()
    pokeb(MLPTRJ+1,0);
    pokeb(KOLOR,0);
    pokeb(PLYMAX,3);
    sargon(api_INITBD);
    sargon_import_position(cp);
    sargon(api_ROYALT);
    pokeb(MOVENO,3);    // Move number is 1 at at start, add 2 to avoid book move
    if( alpha_beta )
        printf("Expect 3 of 15 positions (8,13 and 14) to be skipped\n" );
    else
        printf("Expect all 15 positions 0-14 to be traversed in order\n" );
    nodes.clear();
    sargon(api_CPTRMV);
    bool pass=false;
    if( alpha_beta )
    {
        std::vector<unsigned int> expect = {0,1,2,3,4,5,6,7,9,10,11,12};
        pass = (expect==cardinal_list);
    }
    else
    {
        std::vector<unsigned int> expect = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14};
        pass = (expect==cardinal_list);
    }
    if( !pass )
        ok = false;
    printf( "Expected traverse order (%salpha-beta pruning): %s\n",
                alpha_beta?"":"no ", pass ? "PASS" : "FAIL" );
    char buf[5];
    memcpy( buf, peek(MVEMSG), 4 );
    buf[4] = '\0';
    const char *solution = alpha_beta? "a3a4" : "b3b4";
    pass = (0==strcmp(solution,buf));
    printf( "Simple 15 node best move calculation (%salpha-beta pruning): %s\n",
                alpha_beta?"":"no ", pass ? "PASS" : "FAIL" );
    if( !pass )
    {
        ok = false;
        printf( "FAIL reason: Expected=%s, Calculated=%s\n", solution, buf );
    }
    return ok;
}

bool sargon_whole_game_tests( bool quiet, bool no_very_slow_tests )
{
    bool ok = true;
    printf( "* Whole game tests\n" );
    callback_enabled = false;
    for( int i=0; i<6; i++ )
    {
        int plymax = "334455"[i] - '0'; // two 3 ply games, then 2 4 ply games, then 2 5 ply games
        printf( "Entire PLYMAX=%d 1.%c4 game test ", plymax, i%2==0 ? 'd' : 'e' );
        if( plymax==5 && no_very_slow_tests )
        {
            printf( "** Skipping very slow test **\n" );
            continue;
        }
        std::string game_text;
        std::string between_moves;
        thc::ChessRules cr;
        bool regenerate_position=true;
        int nbr_moves_played = 0;
        unsigned char moveno=1;
        pokeb(MOVENO,moveno);
        while( nbr_moves_played < 200 )
        {
            pokeb(MVEMSG,   0 );
            pokeb(MVEMSG+1, 0 );
            pokeb(MVEMSG+2, 0 );
            pokeb(MVEMSG+3, 0 );
            pokeb(KOLOR,0); // Sargon is white
            pokeb(PLYMAX, plymax );
            if( regenerate_position )
            {
                sargon(api_INITBD);
                sargon_import_position(cr);
                sargon(api_ROYALT);
            }
            nodes.clear();
            sargon(api_CPTRMV);
            thc::ChessRules cr_after;
            sargon_export_position(cr_after);
            std::string terse = sargon_export_move(BESTM);
            thc::Move mv;
            bool ok = mv.TerseIn( &cr, terse.c_str() );
            if( !ok )
            {
                if( !quiet )
                {
                    printf( "Sargon doesn't find move - %s\n%s", terse.c_str(), cr.ToDebugStr().c_str() );
                    printf( "(After)\n%s",cr_after.ToDebugStr().c_str() );
                }
                if( !ok )
                    break;
            }
            std::string s = mv.NaturalOut(&cr);
            if( quiet )
                printf( "." );
            else
                printf( "Sargon plays %s\n", s.c_str() );
            game_text += between_moves;
            game_text += s;
            between_moves = " ";
            cr.PlayMove(mv);
            nbr_moves_played++;
            if( strcmp(cr.squares,cr_after.squares) != 0 )
            {
                printf( "Position mismatch after Sargon move ?!\n" );
                //break;
            }
            std::vector<thc::Move> moves;
            thc::ChessEvaluation ce=cr;
            ce.GenLegalMoveListSorted( moves );
            if( moves.size() == 0 )
            {
                if( !quiet )
                    printf( "Tarrasch static evaluator doesn't find move\n" );
                break;
            }
            mv = moves[0];
            s = mv.NaturalOut(&cr);
            if( !quiet )
                printf( "Tarrasch static evaluator plays %s\n", s.c_str() );
            game_text += " ";
            game_text += s;
            nbr_moves_played++;
            pokeb(COLOR,0x80);
            ok = sargon_play_move(mv);
            cr.PlayMove(mv);
            if( ok )
            {
                sargon_export_position(cr_after);
                if( strcmp(cr.squares,cr_after.squares) != 0 )
                {
                    printf( "Position mismatch after Tarrasch static evaluator move ?!\n" );
                    //break;
                }
            }
            if( moveno <= 254 )
                moveno++;
            pokeb(MOVENO,moveno);
            regenerate_position = !ok;
            if( regenerate_position )
                printf( "Move by move operation has broken down, need to regenerate position ??\n" );
        }
        // We now have introduced book moves (by starting MOVENO at 1 and incrementing it after each pair of half moves), and
        //  they are reproducible by the LDAR callback(). Some games end in repetition with Sargon winning easily, in
        //  particular Sargon does seem to have difficulty mating the opponent if there are too many mates available at higher
        //  plymax
        const char *expected_games[] =
        {
            "d4 d5 Nc3 Nc6 Be3 e5 Nf3 Nxd4 Bxd4 exd4 Qxd4 Nf6 O-O-O Be6 e3 Qd7 Bb5 c6 Ne5 Qc7 Be2 c5 Qa4+ Kd8 Nf3 Be7 Kd2 g6 h4 Kc8 Ng5 Kb8 Nxe6 fxe6 h5 e5 hxg6 a6 Bd3 e4 Nxd5 Nxd5 Qxe4 Rd8 gxh7 Ka7 h8=Q Rxh8 Qxd5 Qb6 Rxh8 Rxh8 Kc1 Rh2 Bc4 Kb8 Qe5+ Ka7 Qxh2 Ka8 Qh8+ Ka7 Qe5 Bd8 Rd6 Qa5 Qd5 Bc7 Rf6 Kb8 c3 Ka7 g3 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7 Rf6 Kb8 Rf8+ Ka7",
            "e4 d5 Qf3 Nf6 Nc3 e6 e5 Ng8 Nh3 Nc6 Bb5 f5 exf6 Nxf6 O-O e5 Re1 Be6 Bxc6+ bxc6 Rxe5 Qd7 Ng5 c5 Nxe6 c6 Nxc5+ Qe7 Rxe7+ Bxe7 Qe3 Kf7 Qe6+ Ke8 Qxc6+ Kf7 Qe6+ Ke8 Nxd5 Nxd5 Qxd5 Rb8 Qe6 Ra8 d4 Rb8 Bg5 Rxb2 Qxe7#",
            "d4 d5 Nc3 Nc6 Bf4 Nf6 Nb5 e5 dxe5 Ne4 e6 Bxe6 Nxc7+ Kd7 Nxa8 Qxa8 Nh3 f5 Qd3 Be7 O-O-O Rg8 f3 Nc5 Qe3 d4 c3 Bxa2 cxd4 Ne6 d5 Nxf4 dxc6+ Kxc6 Qxe7 Qb8 Qd7+ Kb6 Qd4+ Ka5 b4+ Kb5 Nxf4 Qa8 Kb2 Bc4 Qc5+ Ka4 Qxc4 Qb8 Ra1#",
            "e4 d5 Qf3 Nf6 e5 Ne4 d3 Nc5 Nc3 e6 d4 Nca6 Be3 Nc6 O-O-O f5 Nh3 Bb4 Bxa6 bxa6 Bg5 Qd7 a3 Bxc3 Qxc3 O-O Kd2 Bb7 Ke2 a5 b4 a4 b5 Ne7 Qb4 c5 dxc5 Rab8 Qxa4 Ra8 Bf4 Rfe8 c3 Red8 Rhg1 Rdc8 Be3 Rcb8 f4 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8 Rg1 Rc8 Rgf1 Rcb8",
            "d4 d5 Nc3 Nc6 e4 Nf6 e5 Ne4 Be3 g6 Bb5 Nxc3 bxc3 f5 Nf3 Bd7 Bxc6 bxc6 Ng5 Bg7 e6 Bc8 Nf7 Bxd4 Qxd4 Bxe6 Nxd8 Rxd8 Qxh8+ Kd7 Qxh7 c5 Bxc5 Rc8 Qxe7+ Kc6 Qxe6+ Kb7 Rb1+ Ka8 Qxc8#",
            "e4 d5 Nc3 Nf6 e5 Ne4 Qf3 Nxc3 bxc3 Nc6 Bb5 e6 Ne2 f5 O-O Be7 c4 O-O Bb2 Rb8 cxd5 Qxd5 Qxd5 exd5 Bd4 Bd7 Bxa7 Ra8 Bd4 Rab8 Rfb1 Ra8 c3 Rab8 Nf4 Nxd4 Bxd7 Nc6 Nxd5 Bd8 Bxc6 b6 e6 Rc8 e7 Bxe7 Nxe7+ Kh8 Nxc8 Rxc8 Re1 Rb8 Re7 Rc8 Bb7 Rb8 Rxc7 Rd8 d4 Rb8 Rb1 Rd8 Rxb6 Rb8 Bc6 Rxb6 Rc8#"
        };
        std::string expected = expected_games[i];
        bool pass = (game_text == expected);
        if( !pass )
            ok = false;
        printf( " %s\n", pass ? "PASS" : "FAIL" );
        if( !pass )
            printf( "FAIL reason: Expected=%s, Calculated=%s\n", expected.c_str(), game_text.c_str() );
    }
    return ok;
}

struct TEST
{
    const char *fen;
    int plymax_required;
    const char *solution;   // As terse string
};

bool sargon_position_tests( bool quiet, bool no_very_slow_tests )
{
    bool ok = true;
    printf( "* Test position tests\n" );
    callback_enabled = false;

    /*

    Getting the Sargon port working started with this position;
    (test position #1)

    FEN r2n2k1/5ppp/b5q1/1P3N2/8/8/3Q1PPP/3R2K1 w - - 0 1

    r..n..k.
    .....ppp
    b.....q.
    .P...N..
    ........
    ........
    ...Q.PPP
    ...R..K.

    White to play has three forcing wins, requiring
    increasing depth for increasing reward;

    In theory:
    Immediate capture of bishop b5xa6 (wins a piece) [1 ply]
    Royal fork Nf5-e7 (wins queen) [3 ply]
    Back rank mate Qd2xd8+ [4 ply maybe]
    In practice:
    Mate found with PLYMAX 3 or greater
    Royal fork found with PLYMAX 1 or 2

    To make the back rank mate a little harder, move the
    Pb5 and ba6 to Pb3 and ba4 so bishop can postpone mate
    by one move. As expected mate now requires greater
    depth, so royal fork for PLYMAX 1-4, mate if PLYMAX 5

    */

    static TEST tests[]=
    {
        // From a game, expect castling
        //{ "2kr1b1r/1ppq2pp/p1n1bp2/3p4/3Pp2B/2N1P3/PPP1NPPP/R2QK2R w KQ - 2 12", 5, "e1g1" },

        // Initial position, book move
        { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, "d2d4" },

        // Initial position, other random book move
        { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, "e2e4" },

        // Position after 1.d4, Black to play book move
        { "rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq d3 0 1", 5, "d7d5" },
                   
        // Position after 1.c4, Black to play book move
        { "rnbqkbnr/pppppppp/8/8/2P5/8/PP1PPPPP/RNBQKBNR b KQkq c3 0 1", 5, "e7e5" },
                   
        // Test en-passant, black to move
        { "7k/8/8/8/Pp6/4K3/8/8 b - a3 0 1", 5, "b4a3" },

        // CTWBFK Pos 26, page 23 - solution Rc8xc4
        { "2r1r1k1/p3q1pp/bp1pp3/8/2B5/4P3/PP2QPPP/2R2RK1 b - - 0 1", 5, "c8c4" },

        // Test en-passant
        { "8/8/3k4/6Pp/8/8/8/K7 w - h6 0 1", 5, "g5h6" },

        // Point where game test fails, PLYMAX=3 (now fixed, until we tweaked
        //  sargon_import_position_inner() to assume castled kings [if moved] and
        //  making unmoved rooks more likely we got "h1h5")
        { "r4rk1/pR3p1p/8/5p2/2Bp4/5P2/PPP1KPP1/7R w - - 0 18", 3, "h1h5" },
                    // reverted from "e2d3" -> "h1h5 with automatic MOVENO calculation

        // Point where game test fails, PLYMAX=4 (now fixed, until we tweaked
        //  sargon_import_position_inner() to assume castled kings [if moved] and
        //  making unmoved rooks more likely we got "e1f2")
        { "q1k2b1r/pp4pp/2n1b3/5p2/2P2B2/3QPP1N/PP4PP/R3K2R w KQ - 1 15", 4, "e1g1" },

        // Test position #1 above
        { "r2n2k1/5ppp/b5q1/1P3N2/8/8/3Q1PPP/3R2K1 w - - 0 1", 3, "d2d8" },
        { "r2n2k1/5ppp/b5q1/1P3N2/8/8/3Q1PPP/3R2K1 w - - 0 1", 2, "f5e7" },

        // Modified version as discussed above. I used to have a lot of other
        //  slight mods as I tried to figure out what was going wrong before
        //  I got some important things sorted out (most notably the need to
        //  call ROYALT before CPTRMV!)
        { "r2n2k1/5ppp/6q1/5N2/b7/1P6/3Q1PPP/3R2K1 w - - 0 1", 5, "d2d8" },
        { "r2n2k1/5ppp/6q1/5N2/b7/1P6/3Q1PPP/3R2K1 w - - 0 1", 4, "f5e7" },   

        // CTWBFK = "Chess Tactics Workbook For Kids'
        // CTWBFK Pos 30, page 41 - solution Nc3-d5
        { "2r1nrk1/5pbp/1p2p1p1/8/p2B4/PqNR2P1/1P3P1P/1Q1R2K1 w - - 0 1", 5, "c3d5" },

        // CTWBFK Pos 34, page 62 - solution Re7-f7+
        { "5k2/3KR3/4B3/8/3P4/8/8/6q1 w - - 0 1", 5, "e7f7" },
        // CTWBFK Pos 7, page 102 - solution Nc3-d5. For a long time this was a fail
        //  Sargon plays Bd4xb6 instead, so a -2 move instead of a +2 move. Fixed
        //  after adding call to ROYALT() after setting position
        { "3r2k1/1pq2ppp/pb1pp1b1/8/3B4/2N5/PPP1QPPP/4R1K1 w - - 0 1", 5, "c3d5" },

        // CTWBFK Pos 29, page 77 - solution Qe3-a3. Quite difficult!
        { "r4r2/6kp/2pqppp1/p1R5/b2P4/4QN2/1P3PPP/2R3K1 w - - 0 1", 5, "e3a3" },

        // White has Nd3xc5+ pulling victory from the jaws of defeat, it's seen 
        //  It's seen at PLYMAX=3, not seen at PLYMAX=2. It's a kind of 5 ply
        //  calculation (on the 5th half move White captures the queen which is
        //  the only justification for the sac on the 1st half move) - so maybe
        //  in forcing situations add 2 to convert PLYMAX to calculation depth
        { "8/8/q2pk3/2p5/8/3N4/8/4K2R w K - 0 1", 3, "d3c5" },

        // Pawn outside the square needs PLYMAX=5 to solve
        { "3k4/8/8/7P/8/8/1p6/1K6 w - - 0 1", 5, "h5h6" },

        // Pawn one further step back needs, as expected PLYMAX=7 to solve
        { "2k5/8/8/8/7P/8/1p6/1K6 w - - 0 1", 7, "h4h5" },
    
        // Why not play N (either) - d5 mate? (played at PLYMAX=3, but not PLYMAX=5)
        //  I think this is a bug, or at least an imperfection in Sargon. I suspect
        //  something to do with decrementing PLYMAX by 2 if mate found. Our "auto"
        //  mode engine wrapper will mask this, by starting at lower PLYMAX
        { "6B1/2N5/7p/pR4p1/1b2P3/2N1kP2/PPPR2PP/2K5 w - - 0 34", 5, "b5b6" },
        { "6B1/2N5/7p/pR4p1/1b2P3/2N1kP2/PPPR2PP/2K5 w - - 0 34", 3, "c7d5" },

        // CTWBFK Pos 11, page 68 - solution Rf1xf6. Involves quiet moves. Sargon
        //  solves this, but needs PLYMAX 7, takes about 5 mins 45 secs
        { "2rq1r1k/3npp1p/3p1n1Q/pp1P2N1/8/2P4P/1P4P1/R4R1K w - - 0 1", 7, "f1f6" }
    };

    int nbr_tests = sizeof(tests)/sizeof(tests[0]);
    for( int i=0; i<nbr_tests; i++ )
    {
        TEST *pt = &tests[i];
        thc::ChessPosition cp;
        cp.Forsyth(pt->fen);
        pokeb(PLYMAX,pt->plymax_required);
        sargon(api_INITBD);
        sargon_import_position(cp);
        unsigned char color = peekb(COLOR); // who to move? white = 0x00, black = 0x80
        pokeb(KOLOR,color);                 // set Sargon's colour = side to move
        sargon(api_ROYALT);
        if( !quiet )
        {
            sargon_export_position(cp);
            std::string s = cp.ToDebugStr( "Position after test position set" );
            printf( "%s\n", s.c_str() );
        }
        //pokeb(MOVENO,3);    // Move number is 1 at at start, add 2 to avoid book move
    /*  if( i<2 )
        {
            printf( "%s\n", cp.ToDebugStr().c_str() );
            printf( "Test position idx=%d\n", i );
            dbg_position();
        } */
        printf( "Test %d of %d: PLYMAX=%d:", i+1, nbr_tests, pt->plymax_required );
        if( 0 == strcmp(pt->fen,"2rq1r1k/3npp1p/3p1n1Q/pp1P2N1/8/2P4P/1P4P1/R4R1K w - - 0 1") )
        {
            if( no_very_slow_tests )
            {
                printf( "** Skipping very slow test **\n" );
                break;
            }
            else
                printf( " (sorry this particular test is very slow) :" );
        }
        nodes.clear();
        sargon(api_CPTRMV);
        if( !quiet )
        {
            sargon_export_position(cp);
            std::string s = cp.ToDebugStr( "Position after computer move made" );
            printf( "%s\n", s.c_str() );
        }
        std::string sargon_move = sargon_export_move(BESTM);
        bool pass = (sargon_move==std::string(pt->solution));
        printf( " %s\n", pass ? "PASS" : "FAIL" );
        if( !pass )
        {
            ok = false;
            printf( "FAIL reason: Expected=%s, Calculated=%s\n", pt->solution, sargon_move.c_str() );
        }

        // Print move chain
        if( !quiet )
        {
            int nbr = nodes.size();
            int search_start = nbr-1;
            unsigned int target = 1;
            int last_found=0;
            for(;;)
            {
                for( int i=search_start; i>=0; i-- )
                {
                    NODE *n = &nodes[i];
                    unsigned int level = n->level;
                    if( level == target )
                    {
                        last_found = i;
                        unsigned char from  = n->from;
                        unsigned char to    = n->to;
                        unsigned char value = n->value;
                        double fvalue = sargon_export_value(value);
                        printf( "level=%d, from=%s, to=%s value=%d/%.1f\n", n->level, algebraic(from).c_str(), algebraic(to).c_str(), value, fvalue );
                        if( level == pt->plymax_required )
                            break;
                        else
                            target++;
                    }
                }
                if( target == 1 )
                    break;
                else
                {
                    printf("\n");
                    target = 1;
                    search_start = last_found-1;
                }
            }
            for( int i=0; i<nbr; i++ )
            {
                NODE *n = &nodes[i];
                unsigned int level = n->level;
                unsigned char from  = n->from;
                unsigned char to    = n->to;
                unsigned char value = n->value;
                double fvalue = sargon_export_value(value);
                printf( " level=%d, from=%s, to=%s value=%d/%.1f\n", n->level, algebraic(from).c_str(), algebraic(to).c_str(), value, fvalue );
            }
            //diagnostics();
        }
    }
    if( !quiet )
    {
        int offset = MLEND;
        while( offset > 0 )
        {
            unsigned char b = peekb(offset);
            if( b )
            {
                printf("Last non-zero in memory is addr=0x%04x data=0x%02x\n", offset, b );
                break;
            }
            offset--;
        }
    }
    return ok;
}

// Diagnostics dump of chess position from Sargon
void dbg_position()
{
    const unsigned char *sargon_board = peek(BOARDA);
    for( int i=0; i<12; i++ )
    {
        for( int j=0; j<10; j++ )
        {
            unsigned int b = *sargon_board++;
            printf( "%02x", b );
            if( j+1 < 10 )
                printf( " " );
        }
        printf( "\n" );
    }
}

void dbg_ptrs()
{
    printf( "BESTM  = %04x\n", peekw(BESTM) );
    printf( "MLPTRI = %04x\n", peekw(MLPTRI) );
    printf( "MLPTRJ = %04x\n", peekw(MLPTRJ) );
    printf( "MLLST  = %04x\n", peekw(MLLST) );
    printf( "MLNXT  = %04x\n", peekw(MLNXT) );
}

void dbg_score()
{
    printf( "SCORE[0] = %d / %f\n", peekb(SCORE),   sargon_export_value(peekb(SCORE))   );
    printf( "SCORE[1] = %d / %f\n", peekb(SCORE+1), sargon_export_value(peekb(SCORE+1)) );
    printf( "SCORE[2] = %d / %f\n", peekb(SCORE+2), sargon_export_value(peekb(SCORE+2)) );
    printf( "SCORE[3] = %d / %f\n", peekb(SCORE+3), sargon_export_value(peekb(SCORE+3)) );
    printf( "SCORE[4] = %d / %f\n", peekb(SCORE+4), sargon_export_value(peekb(SCORE+4)) );
}

void dbg_plyix()
{
    printf( "PLYIX: " );
    unsigned int p = PLYIX;
    for( int i=0; i<20; i++ )
    {
        unsigned int x =peekw(p);
        p += 2;
        if( x )
            printf( "%04x", x );
        else
            printf( "0" );
        printf( "%s",  i+1<20 ? " " : "\n" );
    }
}

void diagnostics()
{
    dbg_ptrs();
    dbg_plyix();
    dbg_score();
    const char *s="";
    unsigned int p=0;
    for( int k=-1; k<24; k++ )
    {
        switch( k )
        {
            case -1: p = peekw(BESTM);     s = "BESTM";        break;
            case 0:  p = peekw(MLPTRI);    s = "MLPTRI";       break;
            case 1:  p = peekw(MLPTRJ);    s = "MLPTRJ";       break;
            case 2:  p = peekw(MLLST);     s = "MLLST";        break;
            case 3:  p = peekw(MLNXT);     s = "MLNXT";        break;
            case 4:  p = peekw(PLYIX);     s = "PLYIX[0]";     break;
            case 5:  p = peekw(PLYIX+2 );  s = "PLYIX[2]";     break;
            case 6:  p = peekw(PLYIX+4 );  s = "PLYIX[4]";     break;
            case 7:  p = peekw(PLYIX+6 );  s = "PLYIX[6]";     break;
            case 8:  p = peekw(PLYIX+8 );  s = "PLYIX[8]";     break;
            case 9:  p = peekw(PLYIX+10);  s = "PLYIX[10]";    break;
            case 10: p = peekw(PLYIX+12);  s = "PLYIX[12]";    break;
            case 11: p = peekw(PLYIX+14);  s = "PLYIX[14]";    break;
            case 12: p = peekw(PLYIX+16);  s = "PLYIX[16]";    break;
            case 13: p = peekw(PLYIX+18);  s = "PLYIX[18]";    break;
            case 14: p = peekw(PLYIX+20);  s = "PLYIX[20]";    break;
            case 15: p = peekw(PLYIX+22);  s = "PLYIX[22]";    break;
            case 16: p = peekw(PLYIX+24);  s = "PLYIX[24]";    break;
            case 17: p = peekw(PLYIX+26);  s = "PLYIX[26]";    break;
            case 18: p = peekw(PLYIX+28);  s = "PLYIX[28]";    break;
            case 19: p = peekw(PLYIX+30);  s = "PLYIX[30]";    break;
            case 20: p = peekw(PLYIX+32);  s = "PLYIX[32]";    break;
            case 21: p = peekw(PLYIX+34);  s = "PLYIX[34]";    break;
            case 22: p = peekw(PLYIX+36);  s = "PLYIX[36]";    break;
            case 23: p = peekw(PLYIX+38);  s = "PLYIX[38]";    break;
        }
        if( p )
        {
            printf( "%9s: ", s );
            for( int i=0; i<50 && (p); i++ )
            {
                unsigned char from  = peekb(p+2);
                unsigned char to    = peekb(p+3);
                unsigned char flags = peekb(p+4);
                unsigned char value = peekb(p+5);
                double fvalue = sargon_export_value(value);
                if( i > 0 )
                    printf( "%9s: ", " " );
                if( p < 0x400 )
                    printf( "link=0x%04x\n", peekw(p) );
                else
                    printf( "link=0x%04x, from=%s, to=%s flags=0x%02x value=%d/%.1f\n", peekw(p), algebraic(from).c_str(), algebraic(to).c_str(), flags, value, fvalue );
                p = peekw(p);
            }
        }
    }
}

// Data structures to track the 15 positions in minimax example
static std::map<std::string,unsigned int> values;
static std::map<std::string,unsigned int> cardinal_nbr;

// Prime the callback routine to modify node values for simple 15 node minimax
//  algorithm example
void probe_test_prime( bool alpha_beta )
{
    cardinal_nbr["(root)"]  = 0;
    cardinal_nbr["A"]       = 1;
    cardinal_nbr["B"]       = 2;
    cardinal_nbr["AG"]      = 3;
    cardinal_nbr["AH"]      = 4;
    cardinal_nbr["AGB"]     = 5;
    cardinal_nbr["AGA"]     = 6;
    cardinal_nbr["AHB"]     = 7;
    cardinal_nbr["AHA"]     = 8;
    cardinal_nbr["BG"]      = 9;
    cardinal_nbr["BH"]      = 10;
    cardinal_nbr["BGA"]     = 11;
    cardinal_nbr["BGB"]     = 12;
    cardinal_nbr["BHA"]     = 13;
    cardinal_nbr["BHB"]     = 14;

/*

Example 1, no Alpha Beta pruning (move played is 1.b4, value 3.0)

     <-MAX       <-MIN       <-MAX

0-------+-----1-----+-----3-----+------5 
0.0->3.0|   1.a4    |   1...g5  |    2.b4
        | 8.0->2.0  |  7.0->5.0 |       5.0
        |           |           |     <-5.0
        |           |           |     
        |           |           +------6 
        |           |                2.a5
        |           |                   4.0
        |           |               
        |           |               
        |           +-----4-----+------7 
        |              1...h5   |    2.b4
        |              3.0->2.0 |       1.0
        |                 <-2.0 |        
        |                       |     
        |                       +------8 
        |                            2.a5
        |                               2.0
        |                             <-2.0
        |                           
        +------2----+-----9-----+------11
            1.b4    |  1...g5   |    2.a4
          6.0->3.0  |  5.5->4.0 |       4.0
             <-3.0  |           |     <-4.0
                    |           |    
                    |           |    
                    |           +------12
                    |                2.b5
                    |                   3.0
                    |             
                    |             
                    |             
                    +-----10----+------13
                       1...h5   |    2.a4
                       4.0->3.0 |       3.0
                          <-3.0 |     <-3.0
                                |    
                                |    
                                +------14
                                     2.b5
                                        1.0
                                        1.0

*/
    if( !alpha_beta )
    {
        values["(root)"] = sargon_import_value(0.0);
        values["A"]      = sargon_import_value(8.0);
        values["AG"]     = sargon_import_value(7.0);
        values["AGB"]    = sargon_import_value(5.0);
        values["AGA"]    = sargon_import_value(4.0);
        values["AH"]     = sargon_import_value(3.0);
        values["AHB"]    = sargon_import_value(1.0);
        values["AHA"]    = sargon_import_value(2.0);
        values["B"]      = sargon_import_value(6.0);
        values["BG"]     = sargon_import_value(5.5);
        values["BGA"]    = sargon_import_value(4.0);
        values["BGB"]    = sargon_import_value(3.0);
        values["BH"]     = sargon_import_value(4.0);
        values["BHA"]    = sargon_import_value(3.0);
        values["BHB"]    = sargon_import_value(1.0);
    }
/*

Example 2, Alpha Beta pruning (move played is 1.a4, value 5.0)

     <-MAX       <-MIN       <-MAX

0-------+-----1-----+-----3-----+------5 
0.0->5.0|   1.a4    |   1...g5  |    2.b4
        | 6.0->5.0  |  6.0->5.0 |       5.0
        |    <-5.0  |     <-5.0 |     <-5.0
        |           |           |     
        |           |           +------6 
        |           |                2.a5
        |           |                   4.0
        |           |               
        |           |               
        |           +-----4-----+------7 
        |              1...h5   |    2.b4
        |              4.0      |      11.0  Cutoff 11.0>5.0, guarantees 4,7,8 won't affect score of
        |                       |            node 1 (nodes marked with * not evaluated)
        |                       |     
        |                       +------8*
        |                                  
        |                                  
        |                                  
        |                           
        +------2----+-----9-----+------11
            1.b4    |  1...g5   |    2.a4
          6.0->4.0  |  6.0->4.0 |       4.0
                    |     <-4.0 |     <-4.0
                    |           |    
                    |           |    
                    |           +------12
                    |                2.b5
                    |                   3.0  Cutoff, at this point we can be sure node 2 is going to be
                    |                        4.0 or lower, irrespective of nodes 10,13,14, therefore
                    |                        node 2 cannot beat node 1
                    |             
                    +-----10----+------13*
                       1...h5   |    2.a4
                       4.0      |          
                                |    
                                |    
                                |    
                                +------14*
                                     2.b5
                                           

*/

    if( alpha_beta )
    {
        values["(root)"] = sargon_import_value(0.0);
        values["A"]      = sargon_import_value(6.0);
        values["AG"]     = sargon_import_value(6.0);
        values["AGB"]    = sargon_import_value(5.0);
        values["AGA"]    = sargon_import_value(4.0);
        values["AH"]     = sargon_import_value(4.0);
        values["AHB"]    = sargon_import_value(11.0);
        values["AHA"]    = sargon_import_value(2.0);
        values["B"]      = sargon_import_value(6.0);
        values["BG"]     = sargon_import_value(6.0);
        values["BGA"]    = sargon_import_value(4.0);
        values["BGB"]    = sargon_import_value(3.0);
        values["BH"]     = sargon_import_value(4.0);
        values["BHA"]    = sargon_import_value(1.0);
        values["BHB"]    = sargon_import_value(3.0);
    }
}


extern "C" {
    void callback( uint32_t reg_edi, uint32_t reg_esi, uint32_t reg_ebp, uint32_t reg_esp,
                   uint32_t reg_ebx, uint32_t reg_edx, uint32_t reg_ecx, uint32_t reg_eax,
                   uint32_t reg_eflags )
    {
        uint32_t *sp = &reg_edi;
        sp--;

        // expecting code at return address to be 0xeb = 2 byte opcode, (0xeb + 8 bit relative jump),
        uint32_t ret_addr = *sp;
        const unsigned char *code = (const unsigned char *)ret_addr;
        const char *msg = (const char *)(code+2);   // ASCIIZ text should come after that

        if( 0 == strcmp(msg,"LDAR") )
        {
            // For testing purposes, make LDAR output increment, results in
            //  deterministic choice of book moves
            static uint8_t a_reg;
            a_reg++;
            volatile uint32_t *peax = &reg_eax;
            *peax = a_reg;
            return;
        }
        else if( 0 == strcmp(msg,"Yes! Best move") )
        {
            unsigned int  p     = peekw(MLPTRJ);
            unsigned int  level = peekb(NPLY);
            unsigned char from  = peekb(p+2);
            unsigned char to    = peekb(p+3);
            unsigned char flags = peekb(p+4);
            unsigned char value = peekb(p+5);
            NODE n(level,from,to,flags,value);
            nodes.push_back(n);
        }
        if( !callback_enabled )
            return;
        if( std::string(msg) == "After FNDMOV()" )
        {
            if( !callback_quiet )
            {
                printf( "After FNDMOV()\n" );
                diagnostics();
            }
        }

        // For purposes of minimax tracing experiment, we only want two possible
        //  moves in each position - achieved by suppressing King moves
        else if( callback_kingmove_suppressed && std::string(msg) == "Suppress King moves" )
        {
            unsigned char piece = peekb(T1);
            if( piece == 6 )    // King?
            {
                // Change al to 2 and ch to 1 and MPIECE will exit without
                //  generating (non-castling) king moves
                volatile uint32_t *peax = &reg_eax;
                *peax = 2;
                volatile uint32_t *pecx = &reg_ecx;
                *pecx = 0x100;
            }
        }

        // For purposes of minimax tracing experiment, we inject our own points
        //  score for each known position (we keep the number of positions to
        //  managable levels.)
        else if( callback_kingmove_suppressed && std::string(msg) == "end of POINTS()" )
        {
            unsigned int p = peekw(MLPTRJ); // Load move list pointer
            unsigned char from  = p ? peekb(p+2) : 0;
            unsigned char to    = p ? peekb(p+3) : 0;
            unsigned int value = reg_eax&0xff;
            thc::ChessPosition cp;
            sargon_export_position( cp );
            std::string key = convert_to_key( from, to, cp );
            std::string cardinal("??");
            auto it = cardinal_nbr.find(key);
            if( it != cardinal_nbr.end() )
            {                                  
                cardinal_list.push_back(it->second);
                cardinal = util::sprintf( "%d", it->second );
            }
            printf( "Position %s, \"%s\" found\n", cardinal.c_str(), key.c_str() );
            auto it2 = values.find(key);
            if( it2 == values.end() )
                printf( "value not found (?): %s\n", key.c_str() );
            else
            {

                // MODIFY VALUE !
                value = it2->second;
                volatile uint32_t *peax = &reg_eax;
                *peax = value;
            }
            if( !callback_quiet )
            {
                thc::Square sq_from, sq_to;
                bool ok = sargon_export_square(from,sq_from);
                bool root = !ok;
                sargon_export_square(to,sq_to);
                char f1 = thc::get_file(sq_from);
                char f2 = thc::get_rank(sq_from);
                char t1 = thc::get_file(sq_to);
                char t2 = thc::get_rank(sq_to);
                bool was_black = (root || f1=='g' || f1=='h');
                cp.white = was_black;
                static int count;
                std::string s;
                if( root )
                    s = util::sprintf( "%d> value=%d/%.1f", count++, value, sargon_export_value(value) );
                else
                    s = util::sprintf( "%d>. Last move: %c%c%c%c value=%d/%.1f", count++, f1,f2,t1,t2, value, sargon_export_value(value) );
                printf( "%s\n", cp.ToDebugStr(s.c_str()).c_str() );
            }
        }

        // For purposes of minimax tracing experiment, try to figure out
        //  best move calculation
        else if( !callback_quiet && std::string(msg) == "Alpha beta cutoff?" )
        {
            unsigned int al  = reg_eax&0xff;
            unsigned int bx  = reg_ebx&0xffff;
            unsigned int val = peekb(bx);
            bool jmp = (al <= val);
            printf( "Ply level %d\n", peekb(NPLY));
            printf( "Alpha beta cutoff? Yes if move value=%d/%.1f <= two lower ply value=%d/%.1f, ",
                al,  sargon_export_value(al),
                val, sargon_export_value(val) );
            printf( "So %s\n", jmp?"yes":"no" );
        }
        else if( !callback_quiet && std::string(msg) == "No. Best move?" )
        {
            unsigned int al  = reg_eax&0xff;
            unsigned int bx  = reg_ebx&0xffff;
            unsigned int val = peekb(bx);
            bool jmp = (al <= val);
            printf( "Ply level %d\n", peekb(NPLY));
            printf( "Best move? No if move value=%d/%.1f <= one lower ply value=%d/%.1f, ",
                al,  sargon_export_value(al),
                val, sargon_export_value(val) );
            printf( "So %s\n", jmp?"no":"yes" );
        }
        else if( !callback_quiet && std::string(msg) == "Yes! Best move" )
        {
            unsigned int p      = peekw(MLPTRJ);
            unsigned char from  = peekb(p+2);
            unsigned char to    = peekb(p+3);
            printf( "Best move found: %s%s\n", algebraic(from).c_str(), algebraic(to).c_str() );
        }
    }
};


