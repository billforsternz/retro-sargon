/****************************************************************************
 * This project is a Windows port of the classic program Sargon, as
 * presented in the book "Sargon a Z80 Computer Chess Program" by Dan
 * and Kathe Spracklen (Hayden Books 1978).
 *
 * File: sargon-tests.cpp
 *       Position and game regression tests and invoke executable documentation
 *
 * Bill Forster, https://github.com/billforsternz/retro-sargon
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include "util.h"
#include "thc.h"
#include "sargon-asm-interface.h"
#include "sargon-interface.h"
#include "sargon-pv.h"

// Individual tests
bool sargon_position_tests( bool quiet, int comprehensive );
bool sargon_timing_tests( bool quiet, int comprehensive );
bool sargon_whole_game_tests( bool quiet, int comprehensive );
bool sargon_timed_game_test( bool quiet, int comprehensive, bool dummy=false );
extern void sargon_minimax_main();
extern bool sargon_minimax_regression_test( bool quiet);

// Timing calibration game
static std::string timing_calibration_game("e4 e5 Nf3 Nc6 Bb5 Qf6 Nc3 a6 Nd5 Qd6 Bc4 Nf6 O-O b5 Bb3 Nxe4 Re1 Nf6 d4 Nxd5 dxe5 Qg6 Nh4 Qe6 Bxd5 Qe7 Nf5 Qb4 c3 Qc5 Be3 g6 Bxc5 gxf5 e6 Bxc5 exf7+ Kf8 Qh5 Kg7 Qg5+ Kf8 Qh6#");

// main()
int main( int argc, const char *argv[] )
{
#ifdef _DEBUG
    const char *test_args[] =
    {
        "Debug/sargon-tests.exe",
        "c",
        "-3"
    };
    argc = sizeof(test_args) / sizeof(test_args[0]);
    argv = test_args;
#endif
    const char *usage =
    "Sargon test suite\n"
    "\n"
    "Usage:\n"
    "sargon-tests tests [-1|-2|-3] [-v] [-doc]\n"
    "\n"
    "tests = combine 'p' for position tests, 'g' for whole game tests, 'm' for\n"
    "        minimax tests, 't' for timing tests, 'c' for calibrated timing test\n"
    "\n"
    "-1|-2|-3 = fast, middling or comprehensive suite of tests respectively\n"
    "\n"
    "-v means verbose, (i.e. print extra information)\n"
    "\n"
    "-doc means don't run any tests, instead run minimax models and print results\n"
    "     in the form of documentation\n"
    "\n"
    "Examples:\n"
    " sargon-tests pg -3 -v\n"
    "    Run a comprehensive, verbose set of position and whole game tests\n"
    " sargon-tests c -3\n"
    "    Run the calibration test, -3 specifies many iterations for accuracy\n"
    " sargon-tests t\n"
    "    Run timing tests\n"
    " sargon-tests -doc\n"        
    "    Run the minimax models and print out the results as documentation\n";
    bool ok = false, minimax_doc=false, quiet=true;
    std::string test_types;
    int comprehensive = 1;
    for( int i=1; i<argc; i++ )
    {
        std::string s = argv[i];
        if( i==1 && s.find_first_not_of("gptmc") == std::string::npos )
        {
            test_types = s;
            ok = true;
        }
        else if( i==1 && argc==2 && s=="-doc" )
        {
            minimax_doc = true;
            ok = true;
        }
        else if( s=="-1" || s=="-2" || s=="-3" )
        {
            comprehensive = s[1]-'0';   // "-3" -> 3 etc
        }
        else if( s=="-v" )
        {
            quiet = false;
        }
        else
        {
            ok = false;
            break;
        }
    }
    if( !ok )
    {
        printf( usage );
        return -1;
    }

    util::tests();
    if( minimax_doc )
        sargon_minimax_main();
    else
    {
        bool calibration_run  = (test_types.find('c') != std::string::npos);
        bool calibration_only = (test_types=="c");
        int nbr_iterations = comprehensive>1 ? (comprehensive==2?30:100) : 1;
        double elapsed_calibration = 0.0;
        for( int passes=0; passes<(calibration_run?3:1); passes++ )
        {
            if( passes == 1 )
            {
                if( nbr_iterations == 1 )
                {
                    printf( "For timing calibration, an entire 43 ply Level 2 game test from Hans W Kramer.\n" );
                    printf( "For best results, use -2 or -3 for multiple iterations" );
                    if( !quiet )
                        printf( ", don't use the -v flag." );
                    printf( "\n" );
                }
                else
                {
                    printf( "For timing calibration, %d iterations of an entire 43 ply Level 2 game test from Hans W Kramer.\n", nbr_iterations );
                    if( !quiet )
                        printf( "For best results, don't use the -v flag.\n" );
                }
                printf( "One '.' printed for each %s.\n", quiet?"game":"Sargon move" );
            }
            bool ok=true, passed=false;
            std::chrono::time_point<std::chrono::steady_clock> base = std::chrono::steady_clock::now();
            switch( passes )
            {

                // Run most tests
                case 0:
                {
                    for( char c: test_types )
                    {
                        if( c == 'p' )
                        {
                            passed = sargon_position_tests(quiet,comprehensive);
                            if( !passed )
                                ok = false;
                        }
                        else if( c == 't' )
                        {
                            passed = sargon_timing_tests(quiet,comprehensive);
                            if( !passed )
                                ok = false;
                        }
                        else if( c == 'g' )
                        {
                            passed = sargon_whole_game_tests(quiet,comprehensive);
                            if( !passed )
                                ok = false;
                        }
                        else if( c == 'm' )
                        {
                            passed = sargon_minimax_regression_test(quiet);
                            if( !passed )
                                ok = false;
                        }
                    }
                    break;
                }

                // Run calibration test
                case 1:
                {
                    passed = sargon_timed_game_test(quiet,nbr_iterations);
                    if( !passed )
                        ok = false;
                    break;
                }

                // Run dummy calibration test to null out overhead
                case 2:
                {
                    passed = sargon_timed_game_test(quiet,nbr_iterations,true);
                    if( !passed )
                        ok = false;
                    break;
                }
            }
            std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
            std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - base);
            double elapsed = static_cast<double>(ms.count());
            switch( passes )
            {

                // Run most tests
                case 0:
                {
                    if( !calibration_only )
                        printf( "\n%s tests passed. Elapsed time = %.3f seconds\n", ok?"All":"Not all", elapsed/1000.0 );
                    break;
                }

                // Run calibration test
                case 1:
                {
                    printf( "\nCalibration test %s. Elapsed time = %.3f seconds\n", ok?"passed":"failed", elapsed/1000.0 );
                    elapsed_calibration = elapsed;
                    if( !ok )
                        passes++;
                    break;
                }

                // Run dummy calibration test to null out overhead
                case 2:
                {
                    printf( "\nNull out calibration overhead%s. Overhead time = %.3f seconds\n", ok?"":"* FAILS!! *", elapsed/1000.0 );
                    double elapsed_corrected = elapsed_calibration - elapsed;
                    if( ok )
                    {
                        std::string formula;
                        if( nbr_iterations == 1 )
                            formula = util::sprintf("2664 / (%.3f-%.3f)", elapsed_calibration/1000.0, elapsed/1000.0 );
                        else
                            formula = util::sprintf("2664 / ((%.3f-%.3f)/%d)", elapsed_calibration/1000.0, elapsed/1000.0, nbr_iterations);
                        printf(
                            "Hans W Kramer from Switzerland used the Z80 mnemonics assembly generated\n"
                            "as a secondary output of this project to get Sargon running on his 1983\n"
                            "vintage Microprofessor Z80 computer, running at 1.79Mhz. He played this\n"
                            "level 2 game as White against the computer, then automated replays with\n"
                            "an expect script to enable precise timing. On his system Sargon needed\n"
                            "44 minutes and 24 seconds = 2664 seconds to play these moves (44 minutes\n"
                            "and 41 seconds - 17 seconds of overhead). So the speed up ratio on this\n"
                            "machine is;\n%s = %.0f\n"
                            "The speed up ratio on the Acer Aspire 5741G i5 (circa 2010) this program\n"
                            "was developed on was 6030 (+- ~30), tested with -3, no -v.\n"
                            "For reference purposes the moves of the game (eg paste into a GUI, perhaps\n"
                            "Tarrasch) were as follows;\n%s\n",
                            formula.c_str(),
                            (2664 * 1000.0) / ( (double)elapsed_corrected / (double)nbr_iterations ),
                            timing_calibration_game.c_str() );
                    }
                    break;
                }
            }
        }
    }
    return ok ? 0 : -1;
}

bool sargon_whole_game_tests( bool quiet, int comprehensive )
{
    bool ok = true;
    printf( "* Whole game tests\n" );
    int nbr_tests_to_run = 2;
    if( comprehensive > 1 )
        nbr_tests_to_run = comprehensive==2 ? 4 : 6;
    for( int i=0; i<nbr_tests_to_run; i++ )
    {
        int plymax = "334455"[i] - '0'; // two 3 ply games, then 2 4 ply games, then 2 5 ply games
        printf( "Entire PLYMAX=%d 1.%c4 game test ", plymax, i%2==0 ? 'd' : 'e' );
        std::string game_text;
        std::string between_moves;
        thc::ChessRules cr;
        bool regenerate_position=true;
        int nbr_moves_played = 0;
        unsigned char moveno=1;
        pokeb(MOVENO,moveno);
        while( nbr_moves_played < 200 )
        {
            pokeb(KOLOR,0); // Sargon is white
            pokeb(PLYMAX, plymax );
            if( regenerate_position )
                sargon_import_position(cr);
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

bool sargon_timed_game_test( bool quiet, int nbr_iterations, bool dummy )
{
    bool ok = true;
    for( int iteration=0; ok && iteration<nbr_iterations; iteration++ )
    {
        if( !quiet )
            printf( "Run %d of %d (%s run) ", iteration+1, nbr_iterations, dummy?"NULL":"main" );
        std::string game = timing_calibration_game;
        std::vector<thc::Move> moves;
        size_t offset=0;
        thc::ChessRules cr2;
        for(;;)
        {
            std::string move;
            size_t offset2 = game.find(' ',offset);
            if( offset2 == std::string::npos )
                move = game.substr(offset);
            else
            {
                move = game.substr(offset, offset2-offset);
                offset = offset2+1;
            }
            thc::Move mv;
            mv.NaturalIn(&cr2,move.c_str() );
            cr2.PlayMove(mv);
            moves.push_back(mv);
            if( offset2 == std::string::npos )
                break;
        }
        thc::ChessRules cr;
        sargon_import_position(cr);
        unsigned char moveno=1;
        pokeb(MOVENO,moveno);
        pokeb(KOLOR,0x80); // Sargon is black
        pokeb(PLYMAX,2);
        std::string game_text;
        int idx = 0;
        for( thc::Move mv: moves )
        {
            if( !ok )
                break;
            bool white_to_move = ((idx++&1) == 0);
            std::string natural_move;
            thc::ChessRules cr_after;
            if( white_to_move )
            {
                pokeb(COLOR,0); // White to move
                ok = sargon_play_move(mv);
                sargon_export_position(cr_after);
                natural_move = mv.NaturalOut(&cr);
            }
            else
            {
                pokeb(COLOR,0x80); // Black to move
                if( dummy )
                    ok = sargon_play_move(mv);
                else
                {
                    sargon(api_CPTRMV);
                    std::string terse = sargon_export_move(BESTM);
                    ok = mv.TerseIn( &cr, terse.c_str() );
                }
                natural_move = ok ? mv.NaturalOut(&cr) : "??";
                sargon_export_position(cr_after);
                if( moveno <= 254 )
                {
                    moveno++;
                    pokeb(MOVENO,moveno);
                }
            }
            if( ok )
            {
                if( !quiet )
                {
                    printf( "%s", white_to_move?"":"." );
                    //printf( "%s plays %s\n", white_to_move?"Human":"Sargon",natural_move.c_str() );
                }
                if( game_text != "" )
                    game_text += " ";
                game_text += natural_move;
                cr.PlayMove(mv);
                if( strcmp(cr.squares,cr_after.squares) != 0 )
                {
                    printf( "Position mismatch after %s move ?!:\n%s", white_to_move?"Human":"Sargon", cr.ToDebugStr().c_str() );
                    printf( "(This is Sargon's view of the position)\n%s",cr_after.ToDebugStr().c_str() );
                    ok = false;
                }
            }
        }
        printf( quiet ? "." : "\n" );
        if( ok && game_text != game )
        {
            ok = false;
            printf( "\nGame did not complete as expected\n"
                    "Expected: %s\n"
                    "Actual  : %s\n", game.c_str(), game_text.c_str() );
        }
    }
    return ok;
}

struct TEST
{
    const char *fen;
    int plymax_required;
    const char *solution;   // As terse string
    int        centipawns;  // score
    const char *pv;         // As natural (SAN) moves space separated
};

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
    // Position after 1.Nf3, Black to play book move
    { "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 1 1", 5, "d7d5",
        0, "" },
                   
    { "B6k/8/8/8/8/8/8/7K w - - 0 1", 2, "a8d5",
        375, "Bd5 Kg7" },
    { "B6k/8/8/8/8/8/8/7K w - - 0 1", 5, "a8d5",
        375, "Bd5 Kg7 Kg2 Kf6 Kg3" },
    { "b6K/8/8/8/8/8/8/7k b - - 0 1", 2, "a8d5",
        -375, "Bd5 Kg7" },
    { "7k/8/8/8/8/8/8/N6K w - - 0 1", 3, "h1g2",
        375, "Kg2 Kg7 Nb3" },
    { "7K/8/8/8/8/8/8/n6k b - - 0 1", 2, "h1g2",
        -325, "Kg2 Kg7" },
    { "7k/8/8/8/8/8/8/N6K w - - 0 1", 5, "h1g2",
        375, "Kg2 Kg7 Nb3 Kf6 Nc5" },

    // Interesting position, depth 5 why does Sargon think it's so favourable?
    // Sargon 1978 -5.05 (depth 5) 20...Kd8 21.Bb2 e6 22.Nh2 exf5
    // 1r2kb1r/2pbpqp1/p1p2p2/2P2P2/2PP2P1/5N2/P3Q3/R1B2RK1 b k - 0 20
    //   -- leaf position --> 1r1k1b1r/2pb1qp1/p1p2p2/2P2p2/2PP2P1/8/PB2Q2N/R4RK1 w - - 0 23
    // Used this position to investigate and fix BUG_EXTRA_PLY_RESIZE after which the eval
    // is the more sensible 0.75. The bug didn't effect minimax etc. the line, Sargon's
    // internal eval, and the PV was fine, but the eval presented was borked
    { "1r2kb1r/2pbpqp1/p1p2p2/2P2P2/2PP2P1/5N2/P3Q3/R1B2RK1 b k - 0 20", 5, "e8d8",
        0, "Kd8 Bb2 e6 Nh2 exf5" },

    // Same position reversed, same line different eval
    // Sargon 1978 1.65 (depth 5) 20.Kd1 Bb7 21.e3 Nh7 22.exf4
    // r1b2rk1/p3q3/5n2/2pp2p1/2p2p2/P1P2P2/2PBPQP1/1R2KB1R w K - 0 20
    //  -- leaf position --> r4rk1/pb2q2n/8/2pp2p1/2p2P2/P1P2P2/2PB1QP1/1R1K1B1R b - - 0 22
    // Similarly used this position to investigate and fix BUG_EXTRA_PLY_RESIZE after
    // the fix the correct, expected negated eval of -0.75 is reported by the engine.
    // Keep the two positions because it is a nice check that the calculation returns
    // the same result, despite the moves being generated in totally different orders
    // etc.
    { "r1b2rk1/p3q3/5n2/2pp2p1/2p2p2/P1P2P2/2PBPQP1/1R2KB1R w K - 0 20", 5, "e1d1",
        0, "Kd1 Bb7 e3 Nh7 exf4" },

    // This was a real problem - plymax=3/5 generates c7-c5 which is completely illegal - please explain
    // This was a real problem - plymax=1/4 generates d7-d6 also completely illegal (although at least a legal black move!)
    // This was a real problem - plymax=2 generates f6-e5 also completely illegal (although at least a legal black move!)
    { "r4rk1/pb1pq1pp/5p2/2ppP3/5P2/2Q5/PPP3PP/2KR1B1R w - c6 0 15", 2, "c3g3",
        -50, "Qg3 fxe5" },
        // Now fixed. The problem was the en-passant target square. To cope with that
        //  sargon_import_position() was rewinding one half move and trying to play the
        //  move c7-c5, but api_ROYLTY hadn't been called, and so Sargon didn't know
        //  where the Black king was, and it was rejecting c7-c5 as an illegal move
        //  because it thought Black was in check, leaving Sargon's state still with
        //  Black to move. Solution: Incorporate an api_ROYLTY call into
        //  sargon_import_position() 

    // A problem position after testing with Arena (but works okay here)
    { "r1b3kr/pp1R3p/3q2n1/3B4/8/3Q2P1/PP2PP2/R1B1K3 b Q - 0 21", 5, "g8f8",
        1225, "Kf8 Qf5+ Qf6 Qxf6+ Ke8" },

    // This preceding position works okay when testing with Arena
    { "r1b4r/pp1p1Rkp/3q2n1/3B4/8/3Q2P1/PP2PP2/R1B1K3 b Q - 2 20", 5, "g7g8",
        825, "Kg8 Rxd7+ Qxd5 Rxd5 Ne7" },

    // Initial position, book move
    { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, "d2d4",
        0, "" },

    // Initial position, other random book move
    { "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, "e2e4",
        0, "" },

    // Position after 1.d4, Black to play book move
    { "rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq d3 0 1", 5, "d7d5",
        0, "" },
                   
    // Position after 1.c4, Black to play book move
    { "rnbqkbnr/pppppppp/8/8/2P5/8/PP1PPPPP/RNBQKBNR b KQkq c3 0 1", 5, "e7e5",
        0, "" },
                   
    // Test en-passant, black to move
    { "7k/8/8/8/Pp6/4K3/8/8 b - a3 0 1", 5, "b4a3",
        -975, "bxa3 Kd4 a2 Kc5 a1=Q" },

    // CTWBFK Pos 26, page 23 - solution Rc8xc4
    { "2r1r1k1/p3q1pp/bp1pp3/8/2B5/4P3/PP2QPPP/2R2RK1 b - - 0 1", 5, "c8c4",
        -200, "Rxc4 Rxc4 d5 b3 dxc4" },
 
        // Test en-passant
        { "8/8/3k4/6Pp/8/8/8/K7 w - h6 0 1", 5, "g5h6",
        975, "gxh6 Kc7 h7 Kb6 h8=Q" },

    // Point where game test fails, PLYMAX=3 (now fixed, until we tweaked
    //  sargon_import_position_inner() to assume castled kings [if moved] and
    //  making unmoved rooks more likely we got "h1h5")
    { "r4rk1/pR3p1p/8/5p2/2Bp4/5P2/PPP1KPP1/7R w - - 0 18", 3, "h1h5",
        625, "Rh5 Rac8 Kd3" },
                // reverted from "e2d3" -> "h1h5 with automatic MOVENO calculation

    // Point where game test fails, PLYMAX=4 (now fixed, until we tweaked
    //  sargon_import_position_inner() to assume castled kings [if moved] and
    //  making unmoved rooks more likely we got "e1f2")
    { "q1k2b1r/pp4pp/2n1b3/5p2/2P2B2/3QPP1N/PP4PP/R3K2R w KQ - 1 15", 4, "e1g1",
        425, "O-O Be7 Rfd1 Nb4" },

    // Test position #1 above
    { "r2n2k1/5ppp/b5q1/1P3N2/8/8/3Q1PPP/3R2K1 w - - 0 1", 3, "d2d8",
        -1300, "Qxd8+ Rxd8 Rxd8#" },
    { "r2n2k1/5ppp/b5q1/1P3N2/8/8/3Q1PPP/3R2K1 w - - 0 1", 2, "f5e7",
        425, "Ne7+ Kf8" },

    // Modified version as discussed above. I used to have a lot of other
    //  slight mods as I tried to figure out what was going wrong before
    //  I got some important things sorted out (most notably the need to
    //  call ROYALT before CPTRMV!)
    { "r2n2k1/5ppp/6q1/5N2/b7/1P6/3Q1PPP/3R2K1 w - - 0 1", 5, "d2d8",
        1225, "Qxd8+ Be8 Ne7+ Kf8 Nxg6+ fxg6" },
    { "r2n2k1/5ppp/6q1/5N2/b7/1P6/3Q1PPP/3R2K1 w - - 0 1", 4, "f5e7",   
        625, "Ne7+ Kf8 Nxg6+ fxg6" },
 
        // CTWBFK = "Chess Tactics Workbook For Kids'
    // CTWBFK Pos 30, page 41 - solution Nc3-d5
    { "2r1nrk1/5pbp/1p2p1p1/8/p2B4/PqNR2P1/1P3P1P/1Q1R2K1 w - - 0 1", 5, "c3d5",
        100, "Nd5 Qxd5 Bxg7 Qe4 Bxf8" },

    // CTWBFK Pos 34, page 62 - solution Re7-f7+
    { "5k2/3KR3/4B3/8/3P4/8/8/6q1 w - - 0 1", 5, "e7f7",
        975, "Rf7+ Kg8 Rf1+ Kh7 Rxg1" },
         
        // CTWBFK Pos 7, page 102 - solution Nc3-d5. For a long time this was a fail
        //  Sargon plays Bd4xb6 instead, so a -2 move instead of a +2 move. Fixed
        //  after adding call to ROYALT() after setting position
    { "3r2k1/1pq2ppp/pb1pp1b1/8/3B4/2N5/PPP1QPPP/4R1K1 w - - 0 1", 5, "c3d5",
        162, "Nd5 Qxc2 Qxc2 Bxc2 Nxb6" },

    // CTWBFK Pos 29, page 77 - solution Qe3-a3. Quite difficult!
    { "r4r2/6kp/2pqppp1/p1R5/b2P4/4QN2/1P3PPP/2R3K1 w - - 0 1", 5, "e3a3",
        150, "Qa3 Bb5 Rxb5 Qxa3 Rb7+ Rf7" },

    // White has Nd3xc5+ pulling victory from the jaws of defeat, it's seen 
    //  It's seen at PLYMAX=3, not seen at PLYMAX=2. It's a kind of 5 ply
    //  calculation (on the 5th half move White captures the queen which is
    //  the only justification for the sac on the 1st half move) - so maybe
    //  in forcing situations add 2 to convert PLYMAX to calculation depth
    { "8/8/q2pk3/2p5/8/3N4/8/4K2R w K - 0 1", 3, "d3c5",
        275, "Nxc5+ dxc5 Rh6+ Kd7" },

    // Pawn outside the square needs PLYMAX=5 to solve
    { "3k4/8/8/7P/8/8/1p6/1K6 w - - 0 1", 5, "h5h6",
        925, "h6 Kc7 h7 Kb6 h8=Q" },

    // Pawn one further step back needs, as expected PLYMAX=7 to solve
    { "2k5/8/8/8/7P/8/1p6/1K6 w - - 0 1", 7, "h4h5",
        925, "h5 Kb7 h6 Kc6 h7 Kb7 h8=Q" },
    
    // Why not play N (either) - d5 mate? (played at PLYMAX=3, but not PLYMAX=5)
    //  I think this is a bug, or at least an imperfection in Sargon. I suspect
    //  something to do with decrementing PLYMAX by 2 if mate found. Our "auto"
    //  mode engine wrapper will mask this, by starting at lower PLYMAX
    { "6B1/2N5/7p/pR4p1/1b2P3/2N1kP2/PPPR2PP/2K5 w - - 0 34", 5, "b5b6",
        1575, "Rb6 Bxc3 Nd5#" },
    { "6B1/2N5/7p/pR4p1/1b2P3/2N1kP2/PPPR2PP/2K5 w - - 0 34", 3, "c7d5",
        1325, "N7d5#" },

    // CTWBFK Pos 11, page 68 - solution Rf1xf6. Involves quiet moves. Sargon
    //  solves this, but needs PLYMAX 7, takes about 5 mins 45 secs
    { "2rq1r1k/3npp1p/3p1n1Q/pp1P2N1/8/2P4P/1P4P1/R4R1K w - - 0 1", 7, "f1f6",
        -250, "Rxf6 Nxf6 Rf1 Rxc3 bxc3 Qc8 Rxf6" }
        // Interestingly Sargon still thinks 1.Rxf6 is losing, (score -250 centipawns). It only
        // even plays the final 4.Rxf6 in the PV because it banks the material (takes a knight
        // on the last move of the PV and it doesn't fully acknowledge it's going to lose the
        // rook it's just invested). But when you look deeply at what's going on (I did) it
        // sort of makes sense. The defensive half of minimax is working hard to survive until
        // ply 7, so Black has jettisoned material in the PV. The truth is Sargon doesn't quite
        // realise how good 1.Rxf6 is but it does sort of understand it, seeing as it has mated
        // in other lines and black is jettisoning material in the PV.  Sargon is behind in
        // material right from the start so it's PV is at least holding things together (it
        // thinks).
        //
        // I ran it further, to depth 8 and 9 (9 takes several hours) and it was only at depth
        // 9 that Sargon really understands it is winning (Black has jettisoned more material,
        // it is mate next move in the PV). At depth 8 Sargon doesn't play 4.Rxf6 any more
        // because it's not the last move of the PV so Sargon fully appreciates it would be
        // investing a rook and it doesn't see the payoff. Note that the line extends an
        // extra ply at depth 9 because of white's final check.
        //
        //  depth 8 score cp -325; Rxf6 Nxf6 Rf1 Rxc3 bxc3 Kg8 Rb1 Qd7
        //  depth 9 score cp 300;  Rxf6 Nxf6 Rf1 Rc4 Rxf6 Rh4 Qxh4 Kg7 Qh6+ Kg8

};

bool sargon_position_tests( bool quiet, int comprehensive )
{
    bool ok = true;

    // A little test of some of our internal machinery
    thc::ChessPosition cp, cp2;
    std::string en_passant_fen1 = "r4rk1/pb1pq1pp/5p2/2ppP3/5P2/2Q5/PPP3PP/2KR1B1R w - c6 0 15";
    cp.Forsyth( en_passant_fen1.c_str() );   // en passant provides a good workout on import and export
    sargon_import_position(cp);
    sargon_export_position(cp2);
    std::string en_passant_fen2 = cp2.ForsythPublish();
    if( en_passant_fen2 != en_passant_fen1 )
        printf( "Unexpected internal event, expected en_passant_fen2=%s  to equal en_passant_fen1=%s\n", en_passant_fen2.c_str(), en_passant_fen1.c_str() );

    printf( "* Known position tests\n" );
    int nbr_tests = sizeof(tests)/sizeof(tests[0]);
    int nbr_tests_to_run = nbr_tests;
    if( comprehensive < 3 )
        nbr_tests_to_run = comprehensive==2 ? nbr_tests-1 : 10;
    for( int i=0; i<nbr_tests_to_run; i++ )
    {
        TEST *pt = &tests[i];
        thc::ChessRules cr;
        cr.Forsyth(pt->fen);
        if( !quiet )
        {
            std::string intro = util::sprintf("\nTest position %d is", i+1 );
            std::string s = cr.ToDebugStr( intro.c_str() );
            printf( "%s\n", s.c_str() );
            printf( "Expected PV=%s\n", pt->pv );
        }
        printf( "Test %d of %d: PLYMAX=%d:", i+1, nbr_tests_to_run, pt->plymax_required );
        if( 0 == strcmp(pt->fen,"2rq1r1k/3npp1p/3p1n1Q/pp1P2N1/8/2P4P/1P4P1/R4R1K w - - 0 1") )
            printf( " (sorry this particular test is very slow) :" );
        PV pv;
        sargon_run_engine( cr, pt->plymax_required, pv, false );
        std::vector<thc::Move> &v = pv.variation;
        std::string s_pv;
        std::string spacer;
        for( thc::Move mv: v )
        {
            s_pv += spacer;
            spacer = " ";
            s_pv += mv.NaturalOut(&cr);
            cr.PlayMove(mv);
        }
        std::string sargon_move = sargon_export_move(BESTM);
        bool pass = (s_pv==std::string(pt->pv));
        if( !pass )
            printf( "FAIL\n Fail reason: Expected PV=%s, Calculated PV=%s\n", pt->pv, s_pv.c_str() );
        else
        {
            pass = (sargon_move==std::string(pt->solution));
            if( !pass )
                printf( "FAIL\n Fail reason: Expected move=%s, Calculated move=%s\n", pt->solution, sargon_move.c_str() );
            else if( *pt->pv  )
            {
                pass = (pv.value==pt->centipawns);
                if( !pass )
                    printf( "FAIL\n Fail reason: Expected centipawns=%d, Calculated centipawns=%d\n", pt->centipawns, pv.value );
            }
        }
        if( pass )
            printf( " PASS\n" );
        else
            ok = false;
    }
    return ok;
}

bool sargon_timing_tests( bool quiet, int comprehensive )
{
    bool ok = true;
    /*
Hardware needed: one TRS-80 level II with 32k of memory minimum. :lol:

Levels of Play (time per move average)

0 - Immediate response
1 - 20 seconds
2 - 1 minutes
3 - 2 minutes
4 - 6 minutes
5 - 40 minutes
6 - 4 hours "

0 - Immediate response
Level 1: 20 secs
Level 2: 60 secs, 3 x slower vs level 1
Level 3: 120 secs, 2 x slower vs level 2
Level 4: 360 secs, 3 x slower vs level 3
Level 5: 2400 secs, 5 x slower vs level 4
Level 6: 14400 secs, 6 x slower vs level 5


Level 1: .................................. 3400 tests
         Avg=0.001 secs, 15858 x faster vs TRS80
Level 2: ..................................  340 tests
         Avg=0.007 secs, 5.7 x slower vs level 1, 8276 x faster vs TRS-80
Level 3: ..................................   34 tests
         Avg=0.074 secs, 10.2 x slower vs level 2, 1629 x faster vs TRS-80
Level 4: ..................................   34 tests
         Avg=0.291 secs, 3.9 x slower vs level 3, 1238 x faster vs TRS-80
Level 5: ..................................   34 tests
         Avg=3.019 secs, 10.4 x slower vs level 4, 795 x faster vs TRS-80
Level 6: ..................................   34 tests
         Avg=9.467 secs, 3.1 x slower vs level 5, 1521 x faster vs TRS-80
    */


    const char *intro_quiet =
    "Use the -v (verbose) flag for more background information about the following\n"
    "Sargon 1978 move calculation time measurements.\n";

    const char *intro_verbose =
    "On the Talkchess computer chess forum, user mwyoung revealed some Sargon\n"
    "timing information from the (paper) manual that came with an early Z80\n"
    "version of Sargon:\n"
    "Ref: http://talkchess.com/forum3/viewtopic.php?f=2&t=74027&start=40#p866311\n"
    "\n"
    "Hardware needed: one TRS-80 level II with 32k of memory minimum.\n"
    "Level 1: 20 secs (per move)\n"
    "Level 2: 60 secs, 3 x slower vs level 1\n"
    "Level 3: 120 secs, 2 x slower vs level 2\n"
    "Level 4: 360 secs, 3 x slower vs level 3\n"
    "Level 5: 2400 secs, 5 x slower vs level 4\n"
    "Level 6: 14400 secs, 6 x slower vs level 5\n"
    "\n"
    "The following measurements on a variety of positions (the positions used for\n"
    "position tests are reused) serve to provide a comparison to these move times.\n";
    printf( "%s\n", quiet ?intro_quiet:intro_verbose );
    int nbr_tests = sizeof(tests)/sizeof(tests[0]);
    double trs_80 = 1;
    double previous = 1;
    for( int level=1; level<=6; level++ )
    {
        bool level_ok = true;
        int nbr_tests_to_run = nbr_tests;
        if( comprehensive == 1 )
            nbr_tests_to_run = 12;
        else if( comprehensive == 2 )
            nbr_tests_to_run = 20;
        switch( level )
        {
            case 1: trs_80 = 20;        break;
            case 2: trs_80 = 1*60;      break;
            case 3: trs_80 = 2*60;      break;
            case 4: trs_80 = 6*60;      break;
            case 5: trs_80 = 40*60;     break;
            case 6: trs_80 = 4*60*60;   break;
        }
        std::chrono::time_point<std::chrono::steady_clock> base = std::chrono::steady_clock::now();
        std::chrono::time_point<std::chrono::steady_clock> level6_12_tests_base;
        int offset = nbr_tests-nbr_tests_to_run;
        if( offset < 0 )
            offset = 0;  // should never happen, just being cautious
        printf( "Level %d: ", level );
        int multiplier = level<3 ? (level<2?100:10) : 1;
        for( int i=0; i<nbr_tests_to_run; i++ )
        {
            if( level==6 && i+offset==nbr_tests-12 )
                level6_12_tests_base = std::chrono::steady_clock::now();
            for( int j=0; j<multiplier; j++ )
            {
                TEST *pt = &tests[i+offset];
                thc::ChessRules cr;
                cr.Forsyth(pt->fen);
                PV pv;
                sargon_run_engine( cr, level, pv, false );
            }
            printf(".");
        }
        nbr_tests_to_run *= multiplier;
        std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
        std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - base);
        double elapsed = static_cast<double>(ms.count() > 0 ? ms.count() : 1);
        double avg_ms = elapsed/nbr_tests_to_run;
        double avg_s = avg_ms/1000.0;
        double speedup = trs_80/avg_s;
        double exponent = avg_ms / previous;
        previous = avg_ms;
        if( level == 1 )
        {
            printf( " %4d tests\n         Avg=%.3f secs, %.f x faster vs TRS-80\n",
                nbr_tests_to_run, avg_s,
                speedup ); 
        }
        else
        {
            printf( " %4d tests\n         Avg=%.3f secs, %.1f x slower vs level %d, %.f x faster vs TRS-80\n",
                nbr_tests_to_run, avg_s,
                exponent, level-1,
                speedup ); 
        }
        if( level == 6 )
        {
            if( nbr_tests_to_run != 12 )
            {
                ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - level6_12_tests_base);
                elapsed = static_cast<double>(ms.count() > 0 ? ms.count() : 1);
                avg_ms = elapsed/12;
                avg_s = avg_ms/1000.0;
                speedup = trs_80/(avg_s);
            }
            printf( "\nThe Sargon benchmark (=level 6, 12 tests TRS-80 speedup factor) is %.f.\n", speedup );
        }
    }
    if( !quiet )
    {
        const char *postscript =
        "An important caveat to the TRS-80 comparison above is that this is definitely\n"
        "not an apples to apples comparison. How do we know this? The TRS-80 manual\n"
        "entry above also provides an 'instantaneous' time for level 0. But the Sargon\n"
        "1978 source code, and our translation of it to Intel x86 constrains the level\n"
        "to the range 1-6 inclusive. Level 0 is definitely not allowed! Refer to the\n"
        "paragraph of assembly language starting with label IN08:\n\n"
        "Since we are not simply comparing the same software running on different\n"
        "platforms, the comparisons are perhaps moot. With tongue in cheek I have used\n"
        "the measurements to define a 'Sargon' benchmark above which is a measurement\n"
        "of a computer's single core performance. The modest circa 2010 Acer Aspire\n"
        "5741G laptop this software was developed has a benchmark score of ~1140.\n"
        "In light of these considerations, the best estimate of how much faster Sargon\n"
        "1978 runs on a modern PC versus the original Z80 code on Z80 hardware was\n"
        "probably provided by user Tibono on the talkchess forums:\n"
        "Ref: http://talkchess.com/forum3/viewtopic.php?f=2&t=74027&start=10#p845624\n\n"
        "Tibono went above and beyond the call of duty, comparing the x86 UCI engine\n"
        "to original Z80 code running on a Z80 emulator with known timing. He not\n"
        "only verified identical moves were calculated, he hand timed and tabulated\n"
        "the moves. He provided two averages, a speedup ratio of 6917 at level 2 and\n"
        "6125 at level 3. He also used a modest old laptop, he reports it has a CPU &\n"
        "RAM win perf index of 5.8. Sadly a comparable score for my laptop under\n"
        "Windows 10 doesn't seem to be available. My laptop has Windows System\n"
        "Assessment Tool (WinSAT) scores of 8.2 (both CPU and RAM).\n\n"
        "In summary we have considered two independent speedup ratios, on two different\n"
        "modest laptops. One is approx 1000, one is approx 6000. We consider the latter\n"
        "to be a more reliable and better indicator.\n";
        printf( "\n%s", postscript );
    }
    printf( "\n" );
    return ok;
}

static void show()
{
    unsigned char nply = peekb(NPLY);
    if( nply == 1 )
    {
        thc::ChessPosition cp;
        sargon_export_position(cp);
        std::string s = cp.ToDebugStr();
        printf( "%s\n", s.c_str() );
        printf( "NPLY = %02x\n", nply );
        unsigned int addr = PLYIX;
        unsigned int ptr  = peekw(addr);
        printf( "Ply ptrs;\n" );
        for( int i=0; i<4; i++ )
        {
            printf( "%04x", ptr );
            addr += 2;
            ptr  = peekw(addr);
            printf( i+1<4 ? " " : "\n" );
        }
        printf( "MLPTRI=%04x\n", peekw(MLPTRI) );
        printf( "MLPTRJ=%04x\n", peekw(MLPTRJ) );
        printf( "MLLST=%04x\n",  peekw(MLLST) );
        printf( "MLNXT=%04x\n",  peekw(MLNXT) );
        addr = 0x400;
        unsigned int mlnxt = peekw(MLNXT);
        for( int i=0; i<256; i++ )
        {
            ptr  = peekw(addr);
            printf( "%04x: %04x ", addr, ptr );
            printf( "%02x %02x %02x %02x ", peekb(addr+2), peekb(addr+3), peekb(addr+4), peekb(addr+5) );
            printf( "%s\n", sargon_export_move(addr,false).c_str() );
            if( addr == mlnxt )
                break;
            addr += 6;
        }
    }
}

void after_genmov()
{
    //printf( "\nafter genmov()\n" );
    //show();
}
