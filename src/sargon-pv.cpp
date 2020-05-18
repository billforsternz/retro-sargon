/*

  Sargon PV (Principal Variation) Calculation
  
  */

#include <string>
#include <vector>
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
    char adjusted_material;
    char brdc;
    NODE() : level(0), from(0), to(0), adjusted_material(0), brdc(0) {}
    NODE( unsigned int l, unsigned char f, unsigned char t,
          char a, char b ) :
                level(l), from(f), to(t), adjusted_material(a), brdc(b) {}
};

//
//  Build Sargon's PV (Principal Variation)
//

static thc::ChessRules pv_base_position;
static std::vector< NODE > nodes;
static unsigned long max_len_so_far;
static void calculate_pv( PV &pv );
static PV provisional;

void sargon_pv_clear( const thc::ChessPosition &current_position )
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
    //unsigned char flags = peekb(p+4);
    //unsigned char value = peekb(p+5);

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
    //
    //  Also check the git log for ABSOLUTE and RELATIVE to find the
    //  point where the old RELATIVE calculation was removed for
    //  simplicity's sake.

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
    //   checked it and debugged the process here by comparing
    //   adjusted_material to gbl_adjusted_material (now removed)
    //   and making sure it was the same.
    //  The biggest problem was that sometimes (but not always)
    //   Sargon's COLOR had toggled since the POINTS() function ran,
    //   so now we save the value of COLOR in POINTS() [which we
    //   have a callback for].
    char brdc = static_cast<char>(peekb(BRDC));
    if( peekb(PTSCK) )
        brdc = 0;
    NODE n(level,from,to,adjusted_material,brdc);
    nodes.push_back(n);
    if( nodes.size() > max_len_so_far )
        max_len_so_far = nodes.size();
    if( level == 1 )
    {
        calculate_pv( provisional );
        nodes.clear();
    } 
}

// Use our knowledge for the way Sargon does minimax/alpha-beta to build a PV
// When a node is indicated as 'BEST' at level one, we can look back through
//  previously indicated nodes at higher level and construct a PV
static void calculate_pv( PV &pv )
{
    pv.variation.clear();

    // The PV calculation algorithm is derived in the executable documentation within
    //  std::string pv_algorithm_txt in src/sargon-minimax.cpp
    // 
    // Summarising the PV algorithm is;
    // Collect a list of nodes representing all positive 'best move so far' decisions
    // Define an initially empty list PV
    // Set N = 1
    // Scan the best so far node list once in reverse order
    // If a scanned node has level equal to N, append it to PV and increment N
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
                    break;
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
    double centipawns = (4*limit_material + limit_brdc) * 100.0/8.0;
    pv.value = static_cast<int>(centipawns);
}

std::string sargon_pv_report_stats()
{
    return util::sprintf( "max length of build PV vector=%lu\n", max_len_so_far );
}
