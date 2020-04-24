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
#include <chrono>
#include "util.h"
#include "thc.h"
#include "sargon-asm-interface.h"
#include "sargon-interface.h"

// Individual tests
bool sargon_position_tests( bool quiet, int comprehensive );
bool sargon_whole_game_tests( bool quiet, int comprehensive );
extern void sargon_minimax_main();
extern bool sargon_minimax_regression_test( bool quiet);

// Misc diagnostics
void dbg_ptrs();
void dbg_position();
void diagnostics();
void on_exit_diagnostics() {}

// main()
int main( int argc, const char *argv[] )
{
#ifdef _DEBUG
    const char *test_args[] =
    {
        "Debug/sargon-tests.exe",
        "p",
        "-1"
    };
    argc = sizeof(test_args) / sizeof(test_args[0]);
    argv = test_args;
#endif
    const char *usage =
    "Sargon test suite\n"
    "Usage:\n"
    "sargon-tests tests|doc [-1|-2|-3] [-v]\n"
    "\n"
    "tests = combine 'p' for position tests, 'g' for whole game tests, 'm' for minimax tests\n"
    "     OR 'doc' run minimax models and print results in the form of documentation\n"
    "\n"
    "-1|-2|-3 = fast, middling or comprehensive suite of tests respectively\n"
    "\n"
    "-v is verbose\n"
    "\n"
    "Examples:\n"
    " sargon-tests pg -3 -v\n"
    "    Run a comprehensive, verbose set of position and whole game tests\n"
    " sargon-tests doc\n"        
    "    Run the minimax models and print out the results as documentation\n";
    bool ok = false, minimax_doc=false, quiet=true;
    std::string test_types;
    int comprehensive = 1;
    for( int i=1; i<argc; i++ )
    {
        std::string s= argv[i];
        if( !ok && s.find_first_not_of("gpm") == std::string::npos )
        {
            test_types = s;
            ok = true;
        }
        else if( !ok && s=="doc" )
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
        std::chrono::time_point<std::chrono::steady_clock> base = std::chrono::steady_clock::now();
        bool ok=true, passed;
        for( char c: test_types )
        {
            if( c == 'p' )
            {
                passed = sargon_position_tests(quiet,comprehensive);
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
        std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();
        std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - base);
        double elapsed = static_cast<double>(ms.count());
        printf( "%s tests passed. Elapsed time = %.3f seconds\n", ok?"All":"Not all", elapsed/1000.0 ); 
    }
    on_exit_diagnostics();
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

    printf( "* Known position tests\n" );
    static TEST tests[]=
    {
        // Interesting position, depth 5 why does Sargon think it's so favourable?
        // Sargon 1978 -5.05 (depth 5) 20...Kd8 21.Bb2 e6 22.Nh2 exf5
        // 1r2kb1r/2pbpqp1/p1p2p2/2P2P2/2PP2P1/5N2/P3Q3/R1B2RK1 b k - 0 20
        //   -- leaf position --> 1r1k1b1r/2pb1qp1/p1p2p2/2P2p2/2PP2P1/8/PB2Q2N/R4RK1 w - - 0 23
        // Used this position to investigate and fix BUG_EXTRA_PLY_RESIZE after which the eval
        // is the more sensible 0.75. The bug didn't effect minimax etc. the line, Sargon's
        // internal eval, and the PV was fine, but the eval presented was borked
        { "1r2kb1r/2pbpqp1/p1p2p2/2P2P2/2PP2P1/5N2/P3Q3/R1B2RK1 b k - 0 20", 5, "e8d8" },

        // Same position reversed, same line different eval
        // Sargon 1978 1.65 (depth 5) 20.Kd1 Bb7 21.e3 Nh7 22.exf4
        // r1b2rk1/p3q3/5n2/2pp2p1/2p2p2/P1P2P2/2PBPQP1/1R2KB1R w K - 0 20
        //  -- leaf position --> r4rk1/pb2q2n/8/2pp2p1/2p2P2/P1P2P2/2PB1QP1/1R1K1B1R b - - 0 22
        // Similarly used this position to investigate and fix BUG_EXTRA_PLY_RESIZE after
        // the fix the correct, expected negated eval of -0.75 is reported by the engine.
        // Keep the two positions because it is a nice check that the calculation returns
        // the same result, despite the moves being generated in totally different orders
        // etc.
        { "r1b2rk1/p3q3/5n2/2pp2p1/2p2p2/P1P2P2/2PBPQP1/1R2KB1R w K - 0 20", 5, "e1d1" },

        // This was a real problem - plymax=3/5 generates c7-c5 which is completely illegal - please explain
        // This was a real problem - plymax=1/4 generates d7-d6 also completely illegal (although at least a legal black move!)
        // This was a real problem - plymax=2 generates f6-e5 also completely illegal (although at least a legal black move!)
        { "r4rk1/pb1pq1pp/5p2/2ppP3/5P2/2Q5/PPP3PP/2KR1B1R w - c6 0 15", 2, "c3g3" },
            // Now fixed. The problem was the en-passant target square. To cope with that
            //  sargon_import_position() was rewinding one half move and trying to play the
            //  move c7-c5, but api_ROYLTY hadn't been called, and so Sargon didn't know
            //  where the Black king was, and it was rejecting c7-c5 as an illegal move
            //  because it thought Black was in check, leaving Sargon's state still with
            //  Black to move. Solution: Incorporate an api_ROYLTY call into
            //  sargon_import_position() 

        // A problem position after testing with Arena (but works okay here)
        { "r1b3kr/pp1R3p/3q2n1/3B4/8/3Q2P1/PP2PP2/R1B1K3 b Q - 0 21", 5, "g8f8" },

        // This preceding position works okay when testing with Arena
        { "r1b4r/pp1p1Rkp/3q2n1/3B4/8/3Q2P1/PP2PP2/R1B1K3 b Q - 2 20", 5, "g7g8" },

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

/*
Test 1 of 2: PLYMAX=5:Position is
Black to move
.r.k.b.r
..pb.qp.
p.p..p..
..P..p..
..PP..P.
........
PB..Q..N
R....RK.

a) MATERIAL=0x00
a) MATERIAL-PLY0=0x00
a) MATERIAL LIMITED=0x00
a) MOBILITY=0x0e
a) MOBILITY - PLY0=0x04
a) val=0x80
 PASS

Test 2 of 2: PLYMAX=5:Position is
White to move
r....rk.
pb..q..n
........
..pp..p.
..p..P..
P.P..P..
..PB.QP.
.R.K.B.R

b) MATERIAL=0x00
b) MATERIAL-PLY0=0x00
b) MATERIAL LIMITED=0x00
b) MOBILITY=0xf2
b) MOBILITY - PLY0=0xfc
b) val=0x80
 PASS
 */

// A little temporary aid to help us work out eval calculation when working on BUG_EXTRA_PLY_RESIZE
#if 0
    for( int i=0; i<2; i++ )
    {
        double fvalue = sargon_export_value( 0x80 );
        int nbr = 5;

        // Sargon's values are negated at alternate levels, transforming minimax to maximax.
        //  If White to move, maximise conventional values at level 0,2,4
        //  If Black to move, maximise negated values at level 0,2,4
        bool odd = ((nbr-1)%2 == 1);
        bool negate = (i==1 /*WhiteToPlay()*/ ? odd : !odd);
        double centipawns = (negate ? -100.0 : 100.0) * fvalue;

        char mv0 = peekb(MV0);      // net ply 0 material (pawn=2, knight/bishop=6, rook=10...)
        mv0 = 0;
        if( mv0 > 30 )              // Sargon limits this to +-30 (so 15 pawns) to avoid overflow
            mv0 = 30;
        if( mv0 < -30 )
            mv0 = -30;
        char bc0 = peekb(BC0);      // net ply 0 mobility
        bc0 = (i==0 ? 4 : -4);
        if( bc0 > 6 )               // also limited to +-6
            bc0 = 6;
        if( bc0 < -6 )
            bc0 = -6;
        int ply0 = mv0*4 + bc0;     // Material gets 4 times weight as mobility (4*30 + 6 = 126 doesn't overflow signed char)
        double centipawns_ply0 = ply0 * 100.0/8.0;   // pawn is 2*4 = 8 -> 100 centipawns 

        // So actual value is ply0 + score relative to ply0
        int score = static_cast<int>(centipawns_ply0+centipawns);
        printf( "score=%d, centipawns=%f, centipawns_ply0=%f\n", score, centipawns, centipawns_ply0 );
    }
#endif

    int nbr_tests = sizeof(tests)/sizeof(tests[0]);
    int nbr_tests_to_run = nbr_tests;
    if( comprehensive < 3 )
        nbr_tests_to_run = comprehensive==2 ? nbr_tests-1 : 10;
    for( int i=0; i<nbr_tests_to_run; i++ )
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
        printf( "Test %d of %d: PLYMAX=%d:", i+1, nbr_tests_to_run, pt->plymax_required );
        if( 0 == strcmp(pt->fen,"2rq1r1k/3npp1p/3p1n1Q/pp1P2N1/8/2P4P/1P4P1/R4R1K w - - 0 1") )
            printf( " (sorry this particular test is very slow) :" );
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

