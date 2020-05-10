/*

  Sargon PV (Principal Variation) Calculation
  
  */

#define _CRT_SECURE_NO_WARNINGS
#include "util.h"
#include "thc.h"
#include "sargon-interface.h"
#include "sargon-asm-interface.h"
#include "sargon-pv.h"

// A move in Sargon's evaluation graph, in this program a move that is marked as
//  the best move found so far at a given level
struct NODE
{
    unsigned int level;
    unsigned char from;
    unsigned char to;
    unsigned char flags;
    unsigned char value;
    char adjusted_material;
    char brdc;
    NODE() : level(0), from(0), to(0), flags(0), value(0), adjusted_material(0), brdc(0) {}
    NODE( unsigned int l, unsigned char f, unsigned char t,
          unsigned char fs, unsigned char v, char a, char b ) :
                level(l), from(f), to(t), flags(fs), value(v), adjusted_material(a), brdc(b) {}
};

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

/*

  An improved Sargon value/centipawns calculation based on the following
  code in sargon-x86.asm

PT25A:  MOV     al,byte ptr [ebp+PTSL]          ; Get max points lost
        AND     al,al                           ; Is it zero ?
        JZ      rel013                          ; Yes - jump
        DEC     al                              ; Decrement it
rel013: MOV     ch,al                           ; Save it
        MOV     al,byte ptr [ebp+PTSW1]         ; Max,points won
        AND     al,al                           ; Is it zero ?
        JZ      rel014                          ; Yes - jump
        MOV     al,byte ptr [ebp+PTSW2]         ; 2nd max points won
        AND     al,al                           ; Is it zero ?
        JZ      rel014                          ; Yes - jump
        DEC     al                              ; Decrement it
        SHR     al,1                            ; Divide it by 2
rel014: SUB     al,ch                           ; Subtract points lost
        MOV     bx,COLOR                        ; Color of side just moved ???
        TEST    byte ptr [ebp+ebx],80h          ; Is it white ?
        JZ      rel015                          ; Yes - jump
        NEG     al                              ; Negate for black
rel015: MOV     bx,MTRL                         ; Net material on board
        ADD     al,byte ptr [ebp+ebx]           ; Add exchange adjustments
        MOV     _gbl_adjusted_material,al  ;<-- We added this temporarily, now
                                           ;    removed as we have confirmed we
                                           ;    can calculate it using the
                                           ;    callbacks we already have

*/

static unsigned char end_of_points_color;
void sargon_pv_callback_end_of_points()
{
    end_of_points_color = peekb(COLOR);
}

void sargon_pv_callback_yes_best_move()
{
    // Collect the best moves' attributes
    unsigned int p      = peekw(MLPTRJ);
    unsigned int level  = peekb(NPLY);
    unsigned char from  = peekb(p+2);
    unsigned char to    = peekb(p+3);
    unsigned char flags = peekb(p+4);
    unsigned char value = peekb(p+5);

    // In this 'value' is used by Sargon for minimax. It is the value
    //  we convert to and from centipawns in sargon_import_value()
    //  and sargon_export_value(). See also the commentary associated
    //  with sargon_import_value() and sargon_export_value(). This
    //  value cannot be used directly to report the chess engine's
    //  score, as it is relative to ply 0. We need an absolute value.
    //  Teasing out the absolute value is awkward. The score has two
    //  components (material and board control) and these are stored
    //  and limited (i.e. truncated) separately. The errors associated
    //  with truncation can compound if we try to work backwards from
    //  a relative score, so it is best to pick out the absolute
    //  score before it is made relative. That approach eventually
    //  yielded the code below - which is just a C++ conversion of
    //  the sargon assembly code above. See other comments with
    //  the text ABSOLUTE (all caps) in this file.

// PT25A:  MOV     al,byte ptr [ebp+PTSL]          ; Get max points lost
//         AND     al,al                           ; Is it zero ?
//         JZ      rel013                          ; Yes - jump
//         DEC     al                              ; Decrement it
// rel013: MOV     ch,al                           ; Save it
    char ptsl  = static_cast<char>(peekb(PTSL));
    if( ptsl != 0 )
        ptsl--;

//         MOV     al,byte ptr [ebp+PTSW1]         ; Max,points won
//         AND     al,al                           ; Is it zero ?
//         JZ      rel014                          ; Yes - jump
//         MOV     al,byte ptr [ebp+PTSW2]         ; 2nd max points won
//         AND     al,al                           ; Is it zero ?
//         JZ      rel014                          ; Yes - jump
//         DEC     al                              ; Decrement it
//         SHR     al,1                            ; Divide it by 2
    char ptsw1 = static_cast<char>(peekb(PTSW1));
    char ptsw2 = static_cast<char>(peekb(PTSW2));
    char val = ptsw1;
    if( ptsw1 != 0 )
    {
        val = ptsw2;
        if( ptsw2 != 0 )
        {
            val--;
            val /= 2;
        }
    }

// rel014: SUB     al,ch                           ; Subtract points lost
//         MOV     bx,COLOR                        ; Color of side just moved ???
//         TEST    byte ptr [ebp+ebx],80h          ; Is it white ?
//         JZ      rel015                          ; Yes - jump
//         NEG     al                              ; Negate for black
    val -= ptsl;
    //unsigned char color = peekb(COLOR);
    if( (end_of_points_color&0x80) != 0 )
        val = 0 - val;

// rel015: MOV     bx,MTRL                         ; Net material on board
//         ADD     al,byte ptr [ebp+ebx]           ; Add exchange adjustments
//         MOV     _gbl_adjusted_material,al
    char mtrl = static_cast<char>(peekb(MTRL));
    char adjusted_material = (val + mtrl);

    //  It took a few goes to get the calculation working right, we
    //   checked it and debugged the process here. The biggest problem
    //   was that sometimes (but not always) Sargon's COLOR had toggled
    //   since the POINTS() function ran, so now we save the value
    //   of COLOR in POINTS() [which we have a callback for].
#if 0
//extern "C" {
//extern char gbl_adjusted_material;
//};
    if( adjusted_material != gbl_adjusted_material )
    {
        printf( "***** WHOOP WHOOP PULL UP *****\n" );
        printf( "ptsl = %d\n", ptsl );
        printf( "ptsw1 = %d\n", ptsw1 );
        printf( "ptsw2 = %d\n", ptsw2 );
        printf( "val = %d\n", val );
        printf( "color = 0x%02x\n", color );
        printf( "end_of_points_color = 0x%02x\n", end_of_points_color );
        printf( "mtrl = %d\n", mtrl );
        printf( "adjusted_material = %d\n", adjusted_material );
        printf( "gbl_adjusted_material = %d\n", gbl_adjusted_material );
        printf( "***** WHOOP WHOOP PULL UP *****\n" );
    }
#endif // 0
    char brdc = static_cast<char>(peekb(BRDC));
    if( peekb(PTSCK) )
        brdc = 0;
    NODE n(level,from,to,flags,value,adjusted_material,brdc);
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
    NODE *nptr = &nodes_pv[nbr-1];
    double fvalue = sargon_export_value( nptr->value );

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
    //  We no long use the results calculated from this code. Instead of getting an
    //  RELATIVE value and working backwards to get an ABSOLUTE value, we calculate an
    //  ABSOLUTE value directly and use that. Search for ABSOLUTE (in caps) in comments
    //  in this file for more information.
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

    // Don't use this, we don't want exchange adjustment at ply 0
    //double fvalue_ply0 = sargon_export_value( peekb(5) ); //Where root node value ends up if MLPTRJ=0, which it does initially

    // So actual value is ply0 + score relative to ply0
    pv.value2 = static_cast<int>(centipawns_ply0+centipawns);   // pv.value2 is deprecated, we use pv.value based on ABSOLUTE calculation
    //printf( "End of PV position is %s\n", cr.ToDebugStr().c_str() );
    //printf( "Old value=%d, centipawns=%f, centipawns_ply0=%f, plymax=%d\n", pv.value2, centipawns, centipawns_ply0, plymax );

    // Simplified and improved ABSOLUTE value calculation
    int limit_brdc = nptr->brdc;
    if( limit_brdc > 6 )
        limit_brdc = 6;
    else if( limit_brdc < -6 )
        limit_brdc = -6;
    int limit_material = nptr->adjusted_material;
    if( limit_material > 30 )
        limit_material = 30;
    else if( limit_material < -30 )
        limit_material = -30;
    centipawns = (4*limit_material + limit_brdc) * 100.0/8.0;
    pv.value = static_cast<int>(centipawns);   // the old pv.value calculation is still available as pv.value2 but is deprecated
                                               // the new pv.value is now based on the improved ABSOLUTE calculation
    //printf( "New value=%d, centipawns=%f, material=%d brdc=%d\n", pv.value, centipawns, nptr->adjusted_material, nptr->brdc );
}

std::string sargon_pv_report_stats()
{
    return util::sprintf( "max length of build PV vector=%lu\n", max_len_so_far );
}
