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
void probe_test_prime( const char *pos_probe, bool quiet, bool alpha_beta );

int main( int argc, const char *argv[] )
{
    util::tests();
    on_exit_diagnostics();
    bool ok = sargon_tests_quiet();
    if( ok )
        printf( "All tests passed\n" );
    else
        printf( "Not all tests passed\n" );
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
    probe_test_prime(pos_probe,quiet,alpha_beta);
    callback_enabled = true;
    callback_kingmove_suppressed = true;
    callback_quiet = quiet;
    cardinal_list.clear();
    nodes.clear();
    thc::ChessPosition cp;
    cp.Forsyth(pos_probe);
    pokeb(MLPTRJ,0); //need to set this ptr to 0 to get Root position recognised in callback()
    pokeb(MLPTRJ+1,0);
    pokeb(COLOR,0);
    pokeb(KOLOR,0);
    pokeb(PLYMAX,3);
    sargon(api_INITBD);
    sargon_position_import(cp);
    sargon(api_ROYALT);
    pokeb(MOVENO,3);    // Move number is 1 at at start, add 2 to avoid book move
    if( alpha_beta )
        printf("Expect 3 of 15 positions (8,13 and 14) to be skipped\n" );
    else
        printf("Expect all 15 positions 0-14 to be traversed in order\n" );
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
    printf( "Entire PLYMAX=3 game test " );
    for( int i=0; i<3; i++ )
    {
        if( i==2 && no_very_slow_tests )
        {
            printf( "** Not Skipping very slow test **\n" );
            //break;
        }
        std::string game_text;
        std::string between_moves;
        thc::ChessRules cr;
        bool regenerate_position=true;
        int nbr_moves_played = 0;
        while( nbr_moves_played < 200 )
        {
            char buf[5];
            pokeb(MVEMSG,   0 );
            pokeb(MVEMSG+1, 0 );
            pokeb(MVEMSG+2, 0 );
            pokeb(MVEMSG+3, 0 );
            pokeb(COLOR,0);
            pokeb(KOLOR,0);
            pokeb(PLYMAX, i==0 ? 3 : (i==1?4:5) );
            nodes.clear();
            if( regenerate_position )
            {
                sargon(api_INITBD);
                sargon_position_import(cr);
                sargon(api_ROYALT);
                pokeb(MOVENO,3);    // Move number is 1 at at start, add 2 to avoid book move
            }
            sargon(api_CPTRMV);
            thc::ChessRules cr_after;
            sargon_position_export(cr_after);
            memcpy( buf, peek(MVEMSG), 4 );
            buf[4] = '\0';
            bool trigger = false;
            if( std::string(buf) == "OO" )
            {
                strcpy(buf,cr.white?"e1g1":"e8g8");
                trigger = true;
            }
            else if( std::string(buf) == "OOO" )
            {
                strcpy(buf,cr.white?"e1c1":"e8c8");
                trigger = true;
            }
            else if( std::string(buf) == "EP" )
            {
                trigger = true;
                int target = cr.enpassant_target;
                int src=0;
                if( 0<=target && target<64 && cr_after.squares[target] == (cr.white?'P':'p') )
                {
                    if( cr.white )
                    {
                        int src1 = target + 7;
                        int src2 = target + 9;
                        if( cr.squares[src1]=='P' && cr_after.squares[src1]!='P' )
                            src = src1;
                        else
                            src = src2;
                    }
                    else
                    {
                        int src1 = target - 7;
                        int src2 = target - 9;
                        if( cr.squares[src1]=='p' && cr_after.squares[src1]!='p' )
                            src = src1;
                        else
                            src = src2;
                    }
                }
    #define FILE(sq)    ( (char) (  ((sq)&0x07) + 'a' ) )           // eg c5->'c'
    #define RANK(sq)    ( (char) (  '8' - (((sq)>>3) & 0x07) ) )    // eg c5->'5'
                buf[0] = FILE(src);
                buf[1] = RANK(src);
                buf[2] = FILE(target);
                buf[3] = RANK(target);
            }
            thc::Move mv;
            bool ok = mv.TerseIn( &cr, buf );
            if( !ok || trigger )
            {
                if( !quiet )
                {
                    printf( "%s - %s\n%s", ok?"Castling or En-Passant":"Sargon doesnt find move", buf, cr.ToDebugStr().c_str() );
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
/*
;***********************************************************
; PLAYERS MOVE ANALYSIS
;***********************************************************
; FUNCTION:   --  To accept and validate the players move
;                 and produce it on the graphics board. Also
;                 allows player to resign the game by
;                 entering a control-R.
;
; CALLED BY:  --  DRIVER
;
; CALLS:      --  CHARTR
;                 ASNTBI
;                 VALMOV
;                 EXECMV
;                 PGIFND
;                 TBPLCL
;
; ARGUMENTS:  --  None
;***********************************************************
PLYRMV: CALL    CHARTR          ; Accept "from" file letter
        CPI     12H             ; Is it instead a Control-R ?
        JZ      FM09            ; Yes - jump
        MOV     H,A             ; Save
        CALL    CHARTR          ; Accept "from" rank number
        MOV     L,A             ; Save
        CALL    ASNTBI          ; Convert to a board index
        SUB     B               ; Gives board index, if valid
        JRZ     PL08            ; Jump if invalid
        STA     MVEMSG          ; Move list "from" position
        CALL    CHARTR          ; Accept separator & ignore it
        CALL    CHARTR          ; Repeat for "to" position
        MOV     H,A
        CALL    CHARTR
        MOV     L,A
        CALL    ASNTBI
        SUB     B
        JRZ     PL08
        STA     MVEMSG+1        ; Move list "to" position
        CALL    VALMOV          ; Determines if a legal move
        ANA     A               ; Legal ?
        JNZ     PL08            ; No - jump
        CALL    EXECMV          ; Make move on graphics board
        RET                     ; Return
PL08:   LXI     H,LINECT        ; Address of screen line count
        INR     M               ; Increase by 2 for message
        INR     M
        CARRET                  ; New line
        CALL    PGIFND          ; New page if needed
        PRTLIN  INVAL1,12       ; Output "INVALID MOVE"
        PRTLIN  INVAL2,9        ; Output "TRY AGAIN"
        CALL    TBPLCL          ; Tab to players column
        JMP     PLYRMV          ; Jump
        .ENDIF


*/
            cr.PlayMove(mv);
            nbr_moves_played++;
            std::string terse = mv.TerseOut();
            z80_registers regs;
            bool api_ok = false;
            for( int i=0; i<2; i++ )
            {
                memset( &regs, 0, sizeof(regs) );
                unsigned int file = terse[0 + 2*i] - 0x20;    // toupper
                unsigned int rank = terse[1 + 2*i];
                regs.hl = (file<<8) + rank;
                sargon( api_ASNTBI, &regs );
                api_ok = (((regs.bc>>8) & 0xff) == 0);  // ok if reg B eq 0
                if( api_ok )
                {
                    unsigned int board_index = (regs.af&0xff);
                    pokeb(MVEMSG+i,board_index);
                }
            }
            if( api_ok )
            {
                sargon( api_VALMOV, &regs );
                api_ok = ((regs.af & 0xff) == 0);  // ok if reg A eq 0
                if( api_ok )
                {
                    sargon( api_EXECMV, &regs );
                    sargon_position_export(cr_after);
                    if( strcmp(cr.squares,cr_after.squares) != 0 )
                    {
                        printf( "Position mismatch after Tarrasch static evaluator move ?!\n" );
                        //break;
                    }
                }
            }
            regenerate_position = !api_ok;
#if 0
            if( i==0 && nbr_moves_played==34 )  // inspect breakdown point
            {
                printf( "Compare to test position idx=0\n" );
                printf( "%s\n", cr.ToDebugStr().c_str() );
                dbg_position();
            }
            else if( i==1 && nbr_moves_played==28 )  // inspect breakdown point
            {
                printf( "Compare to test position idx=1\n" );
                printf( "%s\n", cr.ToDebugStr().c_str() );
                dbg_position();
            }
#endif
        }
        std::string expected = (i==0 ?
            //"Nc3 d5 d4 Nc6 Be3 e5 dxe5 Nxe5 Qd4 Bd6 Nf3 Nxf3+ exf3 Nf6 Nxd5 O-O Nxf6+ Qxf6"
            //" O-O-O Bd7 Qxf6 gxf6 Bc4 f5 Kd2 c5 Ke2 Bxh2 Rxd7 Be5 Rxb7 Bd4 Bxd4 cxd4 Rh5"
            //" f4 Rg5+ Kh8 Bxf7 Rfd8 Be6 Rdb8 Bd5 Rc8 Be4 h6 Rh7#"
            "Nc3 d5 d4 Nc6 Be3 e5 dxe5 Nxe5 Qd4 Bd6 Nf3 Nxf3+ exf3 Nf6 Nxd5 O-O Nxf6+ Qxf6"
            " O-O-O Bd7 Qxf6 gxf6 Bc4 f5 Kd2 c5 Ke2 Bxh2 Rxd7 Be5 Rxb7 Bd4 Bxd4 cxd4 Kd3"
            " Rab8 Bxf7+ Kh8 Rxa7 Rbd8 Be6 Rb8 Raxh7#"
                : (i==1 ?
            //"Nc3 d5 d4 Nc6 Bf4 Nf6 Nb5 e5 dxe5 Ne4 e6 Bxe6 Nxc7+ Kd7 Nxa8 Qxa8 Nh3 f5 f3 Nc5"
            //" c4 d4 e3 d3 Bxd3 Nxd3+ Qxd3+ Kc8 Kf2 Be7 Rhd1 Rg8 Ng5 Bxg5 Bxg5 Kb8 Qd6+ Kc8 Bf4"
            //" Bxc4 Qd7#"
            "Nc3 d5 d4 Nc6 Bf4 Nf6 Nb5 e5 dxe5 Ne4 e6 Bxe6 Nxc7+ Kd7 Nxa8 Qxa8 Nh3 f5 f3 Nc5"
            " c4 d4 e3 d3 Bxd3 Nxd3+ Qxd3+ Kc8 O-O Be7 Rfd1 Rg8 Ng5 Bxg5 Bxg5 Kb8 Qd6+ Kc8"
            " Bf4 Bxc4 Qd7#"
                :
            //"Nc3 d5 d4 Nc6 Bf4 Nf6 Nb5 e5 Bxe5 Nxe5 dxe5 Ne4 Qxd5 Qxd5 Nxc7+ Kd8 Nxd5 Rb8 f3 Nc5"
            //" O-O-O Ra8 Nb6+ Nd3+ Rxd3+ Bd7 Nxa8 Kc8 Nh3 Kd8 Ng5 Ke8 Nc7+ Ke7 Nxf7 Rg8 Nd6 b6 Ndb5"
            //" a5 Nc3 Kd8 Na8 Bc5 e6 Ke7 Rxd7+ Kxe6 Ra7 h6 e4 g6 Bc4+ Kd6 Bxg8 Ke5 Rb7 b5 Rxb5 Kd4"
            //" Rd1+ Ke3 Rd2 Bb4 Nc7 g5 Rb6 h5 Bf7 h4 a3 Bc5 Ra6 a4 b4 axb3 N7d5#")
            "Nc3 d5 d4 Nc6 Bf4 Nf6 Nb5 e5 Bxe5 Nxe5 dxe5 Ne4 Qxd5 Qxd5 Nxc7+ Kd8 Nxd5 Rb8 f3 Nc5"
            " O-O-O Ra8 Nb6+ Nd3+ Rxd3+ Bd7 Nxa8 Kc8 Nh3 Kd8 Ng5 Ke8 Nc7+ Ke7 Nxf7 Rg8 Nd6 b6 Ndb5"
            " a5 Rd6 Bxb5 Nxb5 Kf7 Rxb6 Be7 Rb7 Rf8 e3 Re8 Nd6+ Kf8 Nxe8 Kxe8 Bd3 g6 h4 Kf7 Kd2"
            " Ke6 Bc4+ Kxe5 Rxe7+ Kd6 Rxh7 Kc5 Kc3 a4 Rg7 g5 hxg5 Kd6 Ra7 Ke5 g4 Kd6 b4 axb3 f4"
            " bxa2 Kb4 a1=Q Rhxa1 Kc6 Rd1 Kb6 Rdd7 Kc6 c3 Kb6 e4 Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4"
            " Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4"
            " Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4"
            " Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4"
            " Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4"
            " Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4 Kc6 Bb5+ Kb6 Bc4"
            " Kc6")
        );
        bool pass = (game_text == expected);
        if( !pass )
            ok = false;
        printf( " %s\n", pass ? "PASS" : "FAIL" );
        if( !pass )
            printf( "FAIL reason: Expected=%s, Calculated=%s\n", expected.c_str(), game_text.c_str() );
        if( i == 0 )
            printf( "Entire PLYMAX=4 game test " );
        else if( i == 1 )
            printf( "Entire PLYMAX=5 game test " );
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
        // Point where game test fails, PLYMAX=3 (now fixed, until we tweaked
        //  sargon_position_import() to assume castled kings [if moved] and
        //  making unmoved rooks more likely we got "h1h5")
        { "r4rk1/pR3p1p/8/5p2/2Bp4/5P2/PPP1KPP1/7R w - - 0 18", 3, "e2d3" },

        // Point where game test fails, PLYMAX=4 (now fixed, until we tweaked
        //  sargon_position_import() to assume castled kings [if moved] and
        //  making unmoved rooks more likely we got "e1f2")
        { "q1k2b1r/pp4pp/2n1b3/5p2/2P2B2/3QPP1N/PP4PP/R3K2R w KQ - 1 15", 4, "OO" },

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
        nodes.clear();
        thc::ChessPosition cp;
        cp.Forsyth(pt->fen);
        pokeb(COLOR,0);
        pokeb(KOLOR,0);
        pokeb(PLYMAX,pt->plymax_required);
        sargon(api_INITBD);
        sargon_position_import(cp);
        sargon(api_ROYALT);
        if( !quiet )
        {
            sargon_position_export(cp);
            std::string s = cp.ToDebugStr( "Position after test position set" );
            printf( "%s\n", s.c_str() );
        }
        pokeb(MOVENO,3);    // Move number is 1 at at start, add 2 to avoid book move
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
        sargon(api_CPTRMV);
        if( !quiet )
        {
            sargon_position_export(cp);
            std::string s = cp.ToDebugStr( "Position after computer move made" );
            printf( "%s\n", s.c_str() );
        }
        char buf[5];
        memcpy( buf, peek(MVEMSG), 4 );
        buf[4] = '\0';
        bool pass = (0==strcmp(pt->solution,buf));
        printf( " %s\n", pass ? "PASS" : "FAIL" );
        if( !pass )
        {
            ok = false;
            printf( "FAIL reason: Expected=%s, Calculated=%s\n", pt->solution, buf );
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
                        target++;
                        last_found = i;
                        unsigned char from  = n->from;
                        unsigned char to    = n->to;
                        unsigned char value = n->value;
                        double fvalue = sargon_value_export(value);
                        printf( "level=%d, from=%s, to=%s value=%d/%.1f\n", n->level, algebraic(from).c_str(), algebraic(to).c_str(), value, fvalue );
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
                double fvalue = sargon_value_export(value);
                printf( "level=%d, from=%s, to=%s value=%d/%.1f\n", n->level, algebraic(from).c_str(), algebraic(to).c_str(), value, fvalue );
            }
            diagnostics();
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
    printf( "SCORE[0] = %d / %f\n", peekb(SCORE),   sargon_value_export(peekb(SCORE))   );
    printf( "SCORE[1] = %d / %f\n", peekb(SCORE+1), sargon_value_export(peekb(SCORE+1)) );
    printf( "SCORE[2] = %d / %f\n", peekb(SCORE+2), sargon_value_export(peekb(SCORE+2)) );
    printf( "SCORE[3] = %d / %f\n", peekb(SCORE+3), sargon_value_export(peekb(SCORE+3)) );
    printf( "SCORE[4] = %d / %f\n", peekb(SCORE+4), sargon_value_export(peekb(SCORE+4)) );
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
                double fvalue = sargon_value_export(value);
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
static std::map<std::string,std::string> positions;
static std::map<std::string,unsigned int> values;
static std::map<std::string,unsigned int> cardinal_nbr;

// Prime the callback routine to modify node values for simple 15 node minimax
//  algorithm example
void probe_test_prime( const char *pos_probe, bool quiet, bool alpha_beta )
{
    thc::ChessPosition cp;
    cp.Forsyth(pos_probe);
    cardinal_nbr["root"]   = 0;
    cardinal_nbr["a4"]     = 1;
    cardinal_nbr["b4"]     = 2;
    cardinal_nbr["a4g5"]   = 3;
    cardinal_nbr["a4h5"]   = 4;
    cardinal_nbr["a4g5b4"] = 5;
    cardinal_nbr["a4g5a5"] = 6;
    cardinal_nbr["a4h5b4"] = 7;
    cardinal_nbr["a4h5a5"] = 8;
    cardinal_nbr["b4g5"]   = 9;
    cardinal_nbr["b4h5"]   = 10;
    cardinal_nbr["b4g5a4"] = 11;
    cardinal_nbr["b4g5b5"] = 12;
    cardinal_nbr["b4h5a4"] = 13;
    cardinal_nbr["b4h5b5"] = 14;

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
        values["root"]   = sargon_value_import(0.0);
        values["a4"]     = sargon_value_import(8.0);
        values["a4g5"]   = sargon_value_import(7.0);
        values["a4g5b4"] = sargon_value_import(5.0);
        values["a4g5a5"] = sargon_value_import(4.0);
        values["a4h5"]   = sargon_value_import(3.0);
        values["a4h5b4"] = sargon_value_import(1.0);
        values["a4h5a5"] = sargon_value_import(2.0);
        values["b4"]     = sargon_value_import(6.0);
        values["b4g5"]   = sargon_value_import(5.5);
        values["b4g5a4"] = sargon_value_import(4.0);
        values["b4g5b5"] = sargon_value_import(3.0);
        values["b4h5"]   = sargon_value_import(4.0);
        values["b4h5a4"] = sargon_value_import(3.0);
        values["b4h5b5"] = sargon_value_import(1.0);
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
        values["root"]   = sargon_value_import(0.0);
        values["a4"]     = sargon_value_import(6.0);
        values["a4g5"]   = sargon_value_import(6.0);
        values["a4g5b4"] = sargon_value_import(5.0);
        values["a4g5a5"] = sargon_value_import(4.0);
        values["a4h5"]   = sargon_value_import(4.0);
        values["a4h5b4"] = sargon_value_import(11.0);
        values["a4h5a5"] = sargon_value_import(2.0);
        values["b4"]     = sargon_value_import(6.0);
        values["b4g5"]   = sargon_value_import(6.0);
        values["b4g5a4"] = sargon_value_import(4.0);
        values["b4g5b5"] = sargon_value_import(3.0);
        values["b4h5"]   = sargon_value_import(4.0);
        values["b4h5a4"] = sargon_value_import(1.0);
        values["b4h5b5"] = sargon_value_import(3.0);
    }
    thc::Move mv;
    thc::ChessPosition base = cp;
    thc::ChessRules work = base;
    std::string pos = std::string(work.squares);
    const char *txt="0 ??0 ??";
    std::string key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "root";

    //  1.a4 g5 2.a5
    work = base;
    mv.TerseIn( &work, txt="a3a4" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "a4";
    mv.TerseIn( &work, txt="g6g5" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "a4g5";
    mv.TerseIn( &work, txt="a4a5" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "a4g5a5";

    //  1.a4 g5 2.b4
    work = base;
    mv.TerseIn( &work, txt="a3a4" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="g6g5" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="b3b4" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "a4g5b4";

    //  1.a4 h5 2.a5 
    work = base;
    mv.TerseIn( &work, txt="a3a4" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="h6h5" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "a4h5";
    mv.TerseIn( &work, txt="a4a5" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "a4h5a5";

    //  1.a4 h5 2.b4
    work = base;
    mv.TerseIn( &work, txt="a3a4" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="h6h5" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="b3b4" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "a4h5b4";

    //  1.b4 h5 2.b5 
    work = base;
    mv.TerseIn( &work, txt="b3b4" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "b4";
    mv.TerseIn( &work, txt="h6h5" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "b4h5";
    mv.TerseIn( &work, txt="b4b5" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "b4h5b5";

    //  1.b4 h5 2.a4
    work = base;
    mv.TerseIn( &work, txt="b3b4" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="h6h5" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="a3a4" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "b4h5a4";

    //  1.b4 g5 2.b5
    work = base;
    mv.TerseIn( &work, txt="b3b4" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="g6g5" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "b4g5";
    mv.TerseIn( &work, txt="b4b5" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "b4g5b5";

    //  1.b4 g5 2.a4
    work = base;
    mv.TerseIn( &work, txt="b3b4" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="g6g5" );
    work.PlayMove( mv );
    mv.TerseIn( &work, txt="a3a4" );
    work.PlayMove( mv );
    pos = std::string(work.squares);
    key = txt;
    key += " -> ";
    key += pos;
    positions[key] = "b4g5a4";
    if( !quiet )
    {
        for( auto it = positions.begin(); it != positions.end(); ++it )
        {
            printf( "%s: %s\n", it->second.c_str(), it->first.c_str() );
        }
    }
}


extern "C" {
    void callback( uint32_t reg_edi, uint32_t reg_esi, uint32_t reg_ebp, uint32_t reg_esp,
                   uint32_t reg_ebx, uint32_t reg_edx, uint32_t reg_ecx, uint32_t reg_eax,
                   uint32_t reg_eflags )
    {
        if( !callback_enabled )
            return;
        uint32_t *sp = &reg_edi;
        sp--;
        uint32_t ret_addr = *sp;
        const unsigned char *code = (const unsigned char *)ret_addr;

        // expecting 0xeb = 2 byte opcode, (0xeb + 8 bit relative jump),
        //  if others skip over 4 operand bytes
        const char *msg = ( code[0]==0xeb ? (char *)(code+2) : (char *)(code+6) );
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
            sargon_position_export( cp );
            std::string sfrom = algebraic(from).c_str();
            std::string sto   = algebraic(to).c_str();
            std::string key = util::sprintf("%s%s -> %s", sfrom.c_str(), sto.c_str(), cp.squares );
            auto it = positions.find(key);
            if( it == positions.end() )
                printf( "key not found (?): %s\n", key.c_str() );
            else
            {
                std::string cardinal("??");
                auto it3 = cardinal_nbr.find(it->second);
                if( it3 != cardinal_nbr.end() )
                {                                  
                    cardinal_list.push_back(it3->second);
                    cardinal = util::sprintf( "%d", it3->second );
                }
                printf( "Position %s, %s found\n", cardinal.c_str(), it->second.c_str() );
                auto it2 = values.find(it->second);
                if( it2 == values.end() )
                    printf( "value not found (?): %s\n", it->second.c_str() );
                else
                {

                    // MODIFY VALUE !
                    value = it2->second;
                    volatile uint32_t *peax = &reg_eax;
                    *peax = value;
                }
            }
            bool was_white = (sfrom[0]=='a' || sfrom[0]=='b');
            cp.white = !was_white;
            if( !callback_quiet )
            {
                static int count;
                std::string s = util::sprintf( "Position %d. Last move: from=%s, to=%s value=%d/%.1f", count++, algebraic(from).c_str(), algebraic(to).c_str(), value, sargon_value_export(value) );
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
            bool jmp = (al < val);
            if( jmp )
            {
                printf( "Ply level %d\n", peekb(NPLY));
                printf( "Alpha beta cutoff? Yes if move value=%d/%.1f < 2 lower ply value=%d/%.1f, ",
                    al,  sargon_value_export(al),
                    val, sargon_value_export(val) );
                printf( "So %s\n", jmp?"yes":"no" );
            }
        }
        else if( !callback_quiet && std::string(msg) == "No. Best move?" )
        {
            unsigned int al  = reg_eax&0xff;
            unsigned int bx  = reg_ebx&0xffff;
            unsigned int val = peekb(bx);
            bool jmp = (al < val);
            if( !jmp )
            {
                printf( "Ply level %d\n", peekb(NPLY));
                printf( "Best move? No if move value=%d/%.1f < 1 lower ply value=%d/%.1f, ",
                    al,  sargon_value_export(al),
                    val, sargon_value_export(val) );
                printf( "So %s\n", jmp?"no":"yes" );
            }
        }
        else if( std::string(msg) == "Yes! Best move" )
        {
            static int best_move_count;
            unsigned int p      = peekw(MLPTRJ);
            unsigned int level  = peekb(NPLY);
            unsigned char from  = peekb(p+2);
            unsigned char to    = peekb(p+3);
            unsigned char flags = peekb(p+4);
            unsigned char value = peekb(p+5);
            NODE n(level,from,to,flags,value);
            nodes.push_back(n);
            if( !callback_quiet )
            {
                printf( "Best move found: %s%s (%d)\n", algebraic(from).c_str(), algebraic(to).c_str(), ++best_move_count );
                //diagnostics();
            }
        }
    }
};

