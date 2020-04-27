/*

  Sargon PV (Principal Variation) Calculation
  
  */

#define _CRT_SECURE_NO_WARNINGS
#include "util.h"
#include "thc.h"
#include "sargon-interface.h"
#include "sargon-asm-interface.h"
#include "sargon-pv.h"

//
//  Build Sargon's PV (Principal Variation)
//

static thc::ChessRules pv_base_position;
static std::vector< NODE > nodes;
static unsigned long max_len_so_far;
static void BuildPV( PV &pv );
static PV provisional;

void sargon_pv_clear( thc::ChessRules &current_position  )
{
    pv_base_position = current_position;
    provisional.clear();
    nodes.clear();
}

PV sargon_pv_get()
{
    return provisional;
}

void sargon_pv_callback_yes_best_move()
{
    unsigned int p      = peekw(MLPTRJ);
    unsigned int level  = peekb(NPLY);
    unsigned char from  = peekb(p+2);
    unsigned char to    = peekb(p+3);
    unsigned char flags = peekb(p+4);
    unsigned char value = peekb(p+5);
    NODE n(level,from,to,flags,value);
    nodes.push_back(n);
    if( nodes.size() > max_len_so_far )
        max_len_so_far = nodes.size();
    if( level == 1 )
    {
        BuildPV( provisional );
        nodes.clear();
    } 
}

// Use our knowledge for the way Sargon does minimax/alpha-beta to build a PV
// When a node is indicated as 'BEST' at level one, we can look back through
//  previously indicated nodes at higher level and construct a PV
static void BuildPV( PV &pv )
{
    pv.variation.clear();
    std::vector<NODE> nodes_pv;
    int nbr = nodes.size();
    int target = 1;
    int plymax = peekb(PLYMAX);
    for( int i=nbr-1; i>=0; i-- )
    {
        NODE *p = &nodes[i];
        if( p->level == target )
        {
            nodes_pv.push_back( *p );
            double fvalue = sargon_export_value(p->value);
            //if( target == plymax ) // commented out to allow extra depth nodes in case of checks - see below *
            //    break;
            target++;
        }
    }
    thc::ChessRules cr = pv_base_position;
    nbr = nodes_pv.size();
    bool ok = true;
    for( int i=0; ok && i<nbr; i++ )
    {
        // See above *
        // Normally we expect plymax nodes. So plymax=3; 3 nodes at level 1,2,3; i=0,1,2.
        // But Sargon does an extra ply if the king is in check, so allow (but don't insist on)
        // extra node(s) if terminal node is check.
        // Example Fen: r1bqk2r/pppp1ppp/2n5/2b4n/4Pp2/2NP1N2/PPPBB1PP/R2QK2R b KQkq - 6 7
        // PV at plymax=5 is: Sargon 1978 -0.97 (depth 5) 7...Qf6 8.Ng1 Bxg1 9.Rxg1 Qh4+ 10.Kf1
        // Before the next paragraph of code was added 10.Kf1 didn't show in the line and the
        // score was -12.80 pawns, the leaf score of Qh4+, which presumably is allowed to be
        // exaggerated because of the check and alpha-beta optimisation (make sure checks are
        // evaluated first).
        if( i >= plymax )
        {
            thc::Square sq = (cr.WhiteToPlay() ? cr.wking_square : cr.bking_square);
            bool in_check = cr.AttackedPiece(sq);
            if( !in_check )
            {
                nodes_pv.resize(i);  // Called this BUG_EXTRA_PLY_RESIZE in the git commit message
                break;
            }
        }
        NODE *p = &nodes_pv[i];
        thc::Square src;
        ok = sargon_export_square( p->from, src );
        if( ok )
        {
            thc::Square dst;
            ok = sargon_export_square( p->to, dst );
            if( ok )
            {
                char buf[5];
                buf[0] = thc::get_file(src);
                buf[1] = thc::get_rank(src);
                buf[2] = thc::get_file(dst);
                buf[3] = thc::get_rank(dst);
                buf[4] = '\0';
                thc::Move mv;
                bool legal = mv.TerseIn( &cr, buf );
                if( !legal )
                {
                    /*
                    log( "Unexpected illegal move=%s, Position=%s\n", buf, cr.ToDebugStr().c_str() );
                    log( "Extra info: Starting position leading to unexpected illegal move FEN=%s, position=\n%s\n",
                        pv_base_position.ForsythPublish().c_str(), pv_base_position.ToDebugStr().c_str() );
                    std::string the_moves = "Moves leading to unexpected illegal move";
                    for( int j=0; j<=i; j++ )
                    {
                        NODE *q = &nodes_pv[j];
                        thc::Square qsrc, qdst;
                        sargon_export_square( q->from, qsrc );
                        sargon_export_square( q->to, qdst );
                        char buff[5];
                        buff[0] = thc::get_file(qsrc);
                        buff[1] = thc::get_rank(qsrc);
                        buff[2] = thc::get_file(qdst);
                        buff[3] = thc::get_rank(qdst);
                        buff[4] = '\0';
                        the_moves += " ";
                        the_moves += std::string(buff);
                    }
                    log( "Extra info: %s\n", the_moves.c_str() ); */
                    break;
                }
                else
                {
                    cr.PlayMove(mv);
                    pv.variation.push_back(mv);
                }
            }
        }
    }

    // Note that nodes_pv might have been resized by in_check test above, so
    //  recalculate nbr (fixing BUG_EXTRA_PLY_RESIZE)
    nbr = nodes_pv.size();
    pv.depth = plymax;
    double fvalue = sargon_export_value( nodes_pv[nbr-1].value );

    // Sargon's values are negated at alternate levels, transforming minimax to maximax.
    //  If White to move, maximise conventional values at level 0,2,4
    //  If Black to move, maximise negated values at level 0,2,4
    bool odd = ((nbr-1)%2 == 1);
    bool negate = pv_base_position.WhiteToPlay() ? odd : !odd;
    double centipawns = (negate ? -100.0 : 100.0) * fvalue;

    // Values are calculated as a weighted combination of net material plus net mobility
    //  plus an adjustment for possible exchanges in the terminal position. The value is
    //  also *relative* to the ply0 score
    //  We want to present the *absolute* value in centipawns, so we need to know the
    //  ply0 score. It is the same weighted combination of net material plus net mobility.
    //  The actual ply0 score is available, but since it also adds the possible exchanges
    //  adjustment and we don't want that, calculate the weighted combination of net
    //  material and net mobility at ply0 instead.
    char mv0 = peekb(MV0);      // net ply 0 material (pawn=2, knight/bishop=6, rook=10...)
    if( mv0 > 30 )              // Sargon limits this to +-30 (so 15 pawns) to avoid overflow
        mv0 = 30;
    if( mv0 < -30 )
        mv0 = -30;
    char bc0 = peekb(BC0);      // net ply 0 mobility
    if( bc0 > 6 )               // also limited to +-6
        bc0 = 6;
    if( bc0 < -6 )
        bc0 = -6;
    int ply0 = mv0*4 + bc0;     // Material gets 4 times weight as mobility (4*30 + 6 = 126 doesn't overflow signed char)
    double centipawns_ply0 = ply0 * 100.0/8.0;   // pawn is 2*4 = 8 -> 100 centipawns 

    // Avoid this apparently simpler alternative, because don't want exchange adjustment at ply 0
#if 0
    double fvalue_ply0 = sargon_export_value( peekb(5) ); //Where root node value ends up if MLPTRJ=0, which it does initially
#endif

    // So actual value is ply0 + score relative to ply0
    pv.value = static_cast<int>(centipawns_ply0+centipawns);
    //printf( "centipawns=%f, centipawns_ply0=%f, plymax=%d\n", centipawns, centipawns_ply0, plymax );
}

std::string sargon_pv_report_stats()
{
    return util::sprintf( "max length of build PV vector=%lu\n", max_len_so_far );
}
