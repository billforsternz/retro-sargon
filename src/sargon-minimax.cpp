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

// Entry points
void sargon_minimax_main();
bool sargon_minimax_regression_test( bool quiet);

// Misc
static std::string get_key();
static std::string overwrite_before_offset( const std::string &s, size_t offset, const std::string &insert );
static std::string overwrite_at_offset( const std::string &s, size_t offset, const std::string &insert );

// Control callback() behaviour
static bool callback_minimax_mods_active;

/*

The big idea
============

This is an attempt to peer into Sargon and see how it works. Hopefully it will
also be a useful exercise for anyone starting out in computer chess programming
to see a working model of Minimax and Alpha-Beta algorithms and "see" how those
algorithms work.

The idea of a "model" is very important. The exponentially exploding complexity
of analysis of a real chess position can overwhelm human attempts to understand
it in detail. So we build a small model instead. Small enought to be easily
understood, large enough to capture all the important concepts.

Our model is a 2x2x2 system. That is in the root position, White to play has
two moves only. In each position reached, Black to play then has two possible
moves. Finally in each of those positions White to play again has just two
possible moves. Counting the root position, only 15 positions are encountered.

We use short "key" notation to describe the positions in our models. "A" and
and "B" are the keys for the positions reached after White's first move. Then
"AG" and "AH" are the positions reached after Black responds with each of
their options after "A". Similarly "BG" and "BH" are the positions after
Black responds to "B". Then "AGA", "AGB" etc. Basically at each turn White's
moves are A and B, Black's are G and H.

To make Sargon co-operate with this idea we tell it to analyse the following
simple position to depth 3 ply;

White king on a1 pawns a4,b3. Black king on h8 pawns g6,h6.

Even this position is more complicated than our simple model, because both
sides have five legal moves rather than two. So we use the Callback facility
to interfere with Sargon's move generator - suppressing all king moves.
Now there are just two moves each at each ply, conveniently for our notation
single square 'a' and 'b' pawn pushes for White and 'g' and 'h' pushes for
Black. The reason the White 'a' pawn is the only one on the fourth rank is
just a pesky detail - it ensures Sargon always generates A moves ahead of
B moves, on the third ply as well as the first.

Of course from a chess perspective nothing is going to be decided in this
position after just 3 ply, pawn promotion will be well over the horizon. So
just as we suppressed Sargon's king moves, we will go ahead and interfere with
Sargon's scoring function using the callback facility, to pretend that the
positions reached have very different scores. While we are at it, we will
pretend that the starting position and the lines of play are quite different
to the actual simple pawn ending, and we'll make up positions and moves that
match the pretend scores. Sargon doesn't need to know!

The cool thing about this with this simple model approach in hand we will be
able to watch Sargon accurately calculate (for example) Philidor's smothered
mate;

We pretend that the initial position is

FEN "1rr4k/4n1pp/7N/8/8/8/Q4PPP/6K1 w - - 0 1",

.rr....k
....n.pp
.......N
........
........
........
Q....PPP
......K.

Our pretend lines and scores are as follows, format is: {key, line, score}

{ "A"  , "1.Qg8+",             0.0   },
{ "AG" , "1.Qg8+ Nxg8",        0.0   },
{ "AGA", "1.Qg8+ Nxg8 2.Nf7#", 12.0  },   // White gives mate
{ "AGB", "1.Qg8+ Nxg8 2.Nxg8", -10.0 },   // Black has huge material plus
{ "AH" , "1.Qg8+ Rxg8",        0.0   },
{ "AHA", "1.Qg8+ Rxg8 2.Nf7#", 12.0  },   // White gives mate
{ "AHB", "1.Qg8+ Rxg8 2.Nxg8", -8.0  },   // Black has large material plus
{ "B"  , "1.Qa1",              0.0   },
{ "BG" , "1.Qa1 Rc6",          0.0   },
{ "BGA", "1.Qa1 Rc6 2.Nf7+",   0.0   },   // equal(ish)
{ "BGB", "1.Qa1 Rc6 2.Ng4",    0.0   },   // equal(ish)
{ "BH" , "1.Qa1 Ng8",          0.0   },
{ "BHA", "1.Qa1 Ng8 2.Nf7#",   12.0  },   // White gives mate
{ "BHB", "1.Qa1 Ng8 2.Ng4",    0.0   }    // equal(ish)

I hope this all makes sense. We'll see exactly this model run later, and
watch Sargon accurately calculate the PV (Principal Variation) as;

PV = 1.Qg8+ Nxg8 2.Nf7#

*/

static std::vector<std::string> big_picture =
{
"    Tree Structure          Creation Order            Minimax Order",
"    ==============          ==============            =============",
"",
"                  AGA                     AGA 5                   AGA 1",
"                 / |                     / |                     / |",
"                AG |                  3 AG |                  3 AG |",
"               /|\\ |                   /|\\ |                   /|\\ |",
"              / | AGB                 / | AGB 6               / | AGB 2",
"             /  |                    /  |                    /  |",
"            A   |                 1 A   |                 7 A   |",
"           /|\\  |                  /|\\  |                  /|\\  |",
"          / | \\ | AHA             / | \\ | AHA 7           / | \\ | AHA 4",
"         /  |  \\|/ |             /  |  \\|/ |             /  |  \\|/ |",
"        /   |   AH |            /   | 4 AH |            /   | 6 AH |",
"       /    |    \\ |           /    |    \\ |           /    |    \\ |",
"      /     |     AHB         /     |     AHB 8       /     |     AHB 5",
"     /      |                /      |                /      |",
"  (root)    |             (root)    |             (root)    |",
"     \\      |                \\      |                \\      |",
"      \\     |     BGA         \\     |     BGA 11      \\     |     BGA 8",
"       \\    |    / |           \\    |    / |           \\    |    / |",
"        \\   |   BG |            \\   | 9 BG |            \\   |10 BG |",
"         \\  |  /|\\ |             \\  |  /|\\ |             \\  |  /|\\ |",
"          \\ | / |  BG             \\ | / | BGB 12          \\ | / | BGB 9",
"           \\|/  |                  \\|/  |                  \\|/  |",
"            B   |                 2 B   |                14 B   |",
"             \\  |                    \\  |                    \\  |",
"              \\ | BHA                 \\ | BHA 13              \\ | BHA 11",
"               \\|/ |                   \\|/ |                   \\|/ |",
"                BH |                 10 BH |                 13 BH |",
"                 \\ |                     \\ |                     \\ |",
"                  BHB                     BHB 14                  BHB 12",
""
};


// Calculate a short string key to represent the moves played
//  eg 1. a4-a5 h6-h5 2. a5-a6 => "AHA"
static std::string get_key()
{
    thc::ChessPosition cp;
    sargon_export_position( cp );
    std::string key = "??";
    int nmoves = 0; // work out how many moves have been played
    bool a0 = (cp.squares[thc::a3] == 'P');
    bool a1 = (cp.squares[thc::a4] == 'P');
    bool a2 = (cp.squares[thc::a5] == 'P');
    if( a1 )
        nmoves += 1;
    else if( a2 )
        nmoves += 2;
    bool b0 = (cp.squares[thc::b4] == 'P');
    bool b1 = (cp.squares[thc::b5] == 'P');
    bool b2 = (cp.squares[thc::b6] == 'P');
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
    if( nmoves == 0 )
    {
        key = "(root)";
    }
    else if( nmoves == 1 )
    {
        if( a1 )
            key = "A";
        else if( b1 )
            key = "B";
    }
    else if( nmoves == 2 )
    {
        if( a1 && g1 )
            key = "AG";
        else if( a1 && h1 )
            key = "AH";
        else if( b1 && g1  )
            key = "BG";
        else if( b1 && h1 )
            key = "BH";
    }
    else if( nmoves == 3 )
    {
        if( a2 && g1 )
            key = "AGA";
        else if( a2 && h1 )
            key = "AHA";
        else if( b2 && g1 )
            key = "BGB";
        else if( b2 && h1 )
            key = "BHB";
        else
        {
            // In other three move cases we can't work out the whole sequence of moves unless we know
            //  the last move, try to rely on this as little as possible
            unsigned int p = peekw(MLPTRJ);  // Load ptr to last move
            unsigned char from  = p ? peekb(p+2) : 0;
            thc::Square sq_from;
            bool ok_from = sargon_export_square(from,sq_from);
            if( ok_from )
            {
                bool a_last = (thc::get_file(sq_from) == 'a');
                bool b_last = (thc::get_file(sq_from) == 'b');
                bool g_last = (thc::get_file(sq_from) == 'g');
                bool h_last = (thc::get_file(sq_from) == 'h');
                if( a1 && g1 && b1 )
                {
                    if ( a_last )
                        key = "BGA";
                    else if ( b_last )
                        key = "AGB";
                }
                else if( a1 && h1 && b1 )
                {
                    if ( a_last )
                        key = "BHA";
                    else if ( b_last )
                        key = "AHB";
                }
            }
        }
    }
    return key;
}

// Create simple models to watch minimax algorithm, they're comprised of move sequences that
//  we map onto our A,B,AG,AH,AGA etc move sequence keys
struct Position
{
    std::string key;
    std::string moves;
    double score;
};

// A complete model; starting position, comments, and a list of all the keyed positions
struct Model
{
    std::string fen;
    std::string comment1;
    std::string comment2;
    std::vector<Position> positions;
};

// Keep track of progress through Sargon's calculation
enum ProgressType {create,eval,alpha_beta_yes,alpha_beta_no,bestmove_yes,bestmove_no,bestmove_confirmed};
struct Progress
{
    ProgressType pt;
    unsigned int move_val;
    unsigned int alphabeta_compare_val;
    unsigned int minimax_compare_val;
    std::string key;
    std::string msg;
    std::string diagram_msg;
};

// Draw nice Ascii Art pictures of the whole calculation
class AsciiArt
{
private:
    std::map<std::string,std::string> lines;
    std::map<std::string,int> key_to_ascii_idx;
    std::map<std::string,size_t> key_to_ascii_offset;
    std::vector<std::string> ascii_art =
    {
        "                    AGA",
        "                   / |",
        "                  /  |",
        "                 AG  |",
        "                /|\\  |",
        "               / | \\ |",
        "              /  |  AGB",
        "             /   |",
        "            A    |",
        "           /|\\   |",
        "          / | \\  |  AHA",
        "         /  |  \\ | / |",
        "        /   |   \\|/  |",
        "       /    |    AH  |",
        "      /     |     \\  |",
        "     /      |      \\ |",
        "    /       |       AHB",
        "   /        |",
        "(root)      |",
        "   \\        |",
        "    \\       |       BGA",
        "     \\      |      / |",
        "      \\     |     /  |",
        "       \\    |    BG  |",
        "        \\   |   /|\\  |",
        "         \\  |  / | \\ |",
        "          \\ | /  |  BGB",
        "           \\|/   |",
        "            B    |",
        "             \\   |",
        "              \\  |  BHA",
        "               \\ | / |",
        "                \\|/  |",
        "                 BH  |",
        "                  \\  |",
        "                   \\ |",
        "                    BHB"
    };

    // Construct ready to go
public:
    AsciiArt( const Model &m )
    {
        for( Position pos: m.positions )
            lines[pos.key]  = pos.moves;
        FindKeys();
        ReplaceKeysWithLines();
    }

    // Add string to end of line
    void Annotate( const std::string &key,  const std::string &annotation )
    {
        int idx = key_to_ascii_idx[key];
        size_t offset = key_to_ascii_offset[key];
        offset += lines[key].length();
        std::string s = ascii_art[idx];
        s = overwrite_at_offset(s,offset,annotation);
        ascii_art[idx] = s;
    }

    // Add asterisk before line
    void Asterisk( const std::string &key )
    {
        int idx = key_to_ascii_idx[key];
        size_t offset = key_to_ascii_offset[key];
        std::string s = ascii_art[idx];
        s = overwrite_before_offset(s,offset,"*");
        ascii_art[idx] = s;
    }

    // Print the diagram
    void Print()
    {
        for( std::string s: ascii_art )
            printf( "%s\n", s.c_str() );
    }

    // Index the keys within the diagram for easy access later
private:
    void FindKeys()
    {
        for( std::pair<std::string,std::string> key_line: lines )
        {
            std::string key = key_line.first;

            // Find the key
            int idx = -1;
            size_t offset ;
            for( unsigned int i=0; i<ascii_art.size(); i++ )
            {
                std::string s = ascii_art[i];
                offset = s.find(key);
                if( offset != std::string::npos )
                {
                    size_t next = offset + key.length();
                    if( next < s.length() && 'A'<= s[next] && s[next]<='H' ) 
                        continue;   // eg key = "AG" found "AGH", keep looking
                    int prev = offset - 1; // int in case offset = 0
                    if( prev >= 0 && 'A'<= s[prev] && s[prev]<='H' ) 
                        continue;   // eg key = "B" found "AGB", keep looking
                    idx = i;
                    break;
                }
            }

            // Should always find the key
            if( idx >= 0 )
            {
                key_to_ascii_idx[key]    = idx;
                key_to_ascii_offset[key] = offset;
            }
            else
                printf( "Unexpected event, key = %s\n", key.c_str() );
        }
    }

    // Replace eg keys "AG" with lines eg "1.Qa1 Rc6"
    void ReplaceKeysWithLines()
    {
        for( std::pair<std::string,std::string> key_line: lines )
        {
            std::string key  = key_line.first;
            std::string line = key_line.second;
            int idx = key_to_ascii_idx[key];
            size_t offset = key_to_ascii_offset[key];
            std::string s = ascii_art[idx];
            std::string t = overwrite_at_offset( s, offset, line );
            ascii_art[idx] = t;
        }
    }
};

// A complete example runs a model and presents the results
class Example
{
private:
    const Model &model;
    std::map<std::string,double> scores;
    std::string pv_key;
    AsciiArt ascii_art;

    // Some variables accessed by callback() need to be visible
public:
    std::map<std::string,unsigned int> values;
    std::map<std::string,unsigned int> cardinal_nbr;
    std::map<std::string,std::string> lines;
    std::vector<Progress> progress;

public:
    // Set up the example
    Example( const Model &m ) : ascii_art(m), model(m)
    {
        cardinal_nbr["(root)"]  = 0;
        cardinal_nbr["A"]       = 1;
        cardinal_nbr["B"]       = 2;
        cardinal_nbr["AG"]      = 3;
        cardinal_nbr["AH"]      = 4;
        cardinal_nbr["AGA"]     = 5;
        cardinal_nbr["AGB"]     = 6;
        cardinal_nbr["AHA"]     = 7;
        cardinal_nbr["AHB"]     = 8;
        cardinal_nbr["BG"]      = 9;
        cardinal_nbr["BH"]      = 10;
        cardinal_nbr["BGA"]     = 11;
        cardinal_nbr["BGB"]     = 12;
        cardinal_nbr["BHA"]     = 13;
        cardinal_nbr["BHB"]     = 14;
        for( Position pos: model.positions )
        {
            lines[pos.key]  = pos.moves;
            scores[pos.key] = pos.score;
            values[pos.key] = sargon_import_value(pos.score); 
        }
    }

    // Run the example
    void Run()
    {
        progress.clear();
        callback_minimax_mods_active = true;

        // White king on a1 pawns a4,b3 Black king on h8 pawns g6,h6 we are going
        //  to use this very dumb position to probe Alpha Beta pruning etc. (we
        //  will kill the kings so that each side has only two moves available
        //  at each position).
        // (Start 'a' pawn on a4 instead of a3 so that 'a' pawn move is generated
        // first on White's second move, even if 'b' pawn advances on first move)
        const char *pos_probe = "7k/8/6pp/8/1P6/P7/8/K7 w - - 0 1";

        // Because there are only 2 moves available at each ply, we can explore
        //  to PLYMAX=3 with only 2 positions at ply 1, 4 positions at ply 2
        //  and 8 positions at ply 3 (plus 1 root position at ply 0) for a very
        //  manageable 1+2+4+8 = 15 nodes (i.e. positions) total. We use the
        //  callback facility to monitor the algorithm and indeed actively
        //  interfere with it by changing the node evals and watching how that
        //  effects node traversal and generates a best move.
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
        sargon(api_CPTRMV);
        callback_minimax_mods_active = false;
    }

    // Print introduction
    void PrintIntro( int example_nbr )
    {
        // Show position and print initial comment
        printf("\n");
        printf( "Example number %d)\n", example_nbr );
        printf( "-----------------%s\n", example_nbr>=10?"-":"" );
        thc::ChessPosition cp;
        cp.Forsyth( model.fen.c_str() );
        printf( "%s\n", cp.ToDebugStr(model.comment1.c_str()).c_str() );
    }

    // Annotate ascii art lines with progress through minimax calculation
    void Annotate()
    {
        int order = 1;
        std::string eval_key;
        std::string move_score;
        std::string alphabeta_mini_msg;
        for( Progress &prog: progress )
        {
            std::string key;
            std::string msg;
            std::string neg_float_value;
            std::string float_value;
            switch( prog.pt )
            {
                case eval:
                    eval_key = prog.key;
                    move_score = util::sprintf( "%.1f", sargon_export_value(prog.move_val) );
                    float_value = (prog.alphabeta_compare_val==0 ? "MAX" : util::sprintf("%.1f",sargon_export_value(prog.alphabeta_compare_val)) ); // Show "MAX" instead of "12.8"
                    neg_float_value = (prog.minimax_compare_val==0 ? "-MAX" : util::sprintf("%.1f",0.0-sargon_export_value(prog.minimax_compare_val)) ); // Show "-MAX" instead of "-12.8"
                    alphabeta_mini_msg = util::sprintf( " [%s,%s] ", float_value.c_str(), neg_float_value.c_str() );
                    key = eval_key;
                    break;
                case alpha_beta_yes:
                    key = eval_key;
                    msg = move_score + alphabeta_mini_msg + move_score + prog.diagram_msg;
                    break;
                case alpha_beta_no:
                    break;
                case bestmove_yes:
                    prog.key = eval_key;
                    key = prog.key;
                    msg = move_score + alphabeta_mini_msg + move_score + prog.diagram_msg;
                    break;
                case bestmove_no:
                    key = eval_key;
                    msg = move_score + alphabeta_mini_msg + move_score + prog.diagram_msg;
                    break;
            }
            if( msg != "" )
            {
                std::string insert = util::sprintf(" (%d): %s",order++,msg.c_str());
                ascii_art.Annotate(key,insert);
            }
        }
    }

    // Run PV algorithm, asterisk the PV nodes in ascii art
    void CalculatePV()
    {
        // Loop in reverse order, scanning for best move choices at level 1 (then 2 then 3)
        int target = 1;
        for( std::vector<Progress>::reverse_iterator it=progress.rbegin();  it!=progress.rend(); ++it )
        {
            if( it->pt == bestmove_yes )
            {
                std::string key = it->key;
                if( target == it->key.length() )
                {
                    target++;

                    // Found, note that last key found handily encodes the whole PV, eg if PV is
                    // "B", "BG", "BGH", then last pv_key will be "BGH"
                    pv_key = key;
                    ascii_art.Asterisk(key);
                }
            }
        }
    }

    // Print ascii art diagram
    void PrintDiagram()
    {
        ascii_art.Print();
    }

    // Print conclusion
    void PrintConclusion()
    {
        // Print PV
        printf( "\nPV = %s, note that the PV nodes are asterisked\n",  lines[pv_key].c_str() );

        // Print concluding comment
        printf( "%s\n", model.comment2.c_str() );

        // Print textual summary
        printf( "\nDetailed log\n" );
        for( Progress prog: progress )
            printf( "%s\n", prog.msg.c_str() );
    }

    // Detailed log
    std::string DetailedLog()
    {
        std::string s;
        for( Progress prog: progress )
        {
            s += prog.msg;
            s += '\n';
        }
        return s;
    }

};

// White can take a bishop or fork king queen and rook
static Model model1 =
{
   "7k/R1ppp1p1/1p6/4q3/5N1r/3b3P/PP3PP1/Q5K1 w - - 0 1",
   "White can take a bishop or fork king queen and rook",
   "There are no Alpha-Beta cutoffs, PV shows white correctly winning queen",
    {
       { "A"     , "1.Nxd3",                0.0  },
       { "AG"    , "1.Nxd3 Qd6",            0.0  },
       { "AGA"   , "1.Nxd3 Qd6 2.Ne1",      3.3  },    // White wins a bishop
       { "AGB"   , "1.Nxd3 Qd6 2.Qb1",      3.0  },    // White wins a bishop
       { "AH"    , "1.Nxd3 Qg5",            0.0  },
       { "AHA"   , "1.Nxd3 Qg5 2.Rxc7",     3.1  },    // White wins a bishop
       { "AHB"   , "1.Nxd3 Qg5 2.Kh2",      3.2  },    // White wins a bishop
       { "B"     , "1.Ng6+",                0.0  },
       { "BG"    , "1.Ng6+ Kh7",            0.0  },
       { "BGA"   , "1.Ng6+ Kh7 2.Nxe5",     9.2  },    // White wins a queen
       { "BGB"   , "1.Ng6+ Kh7 2.Nxh4",     5.0  },    // White wins a rook
       { "BH"    , "1.Ng6+ Kg8",            0.0  },
       { "BHA"   , "1.Ng6+ Kg8 2.Nxe5",     9.0  },    // White wins a queen
       { "BHB"   , "1.Ng6+ Kg8 2.Nxh4",     5.2  }     // White wins a rook
    }
};

// White can give Philidor's mate, or defend
static Model model2 =
{
    "1rr4k/4n1pp/7N/8/8/8/Q4PPP/6K1 w - - 0 1",
    "White can give Philidor's mate, or defend",
    "Philidor's mating line comes first, so plenty of alpha-beta cutoffs",
    {
        { "A"     , "1.Qg8+",               0.0   },
        { "AG"    , "1.Qg8+ Nxg8",          0.0   },
        { "AGA"   , "1.Qg8+ Nxg8 2.Nf7#",   12.0  },   // White gives mate
        { "AGB"   , "1.Qg8+ Nxg8 2.Nxg8",   -10.0 },   // Black has huge material plus
        { "AH"    , "1.Qg8+ Rxg8",          0.0   },
        { "AHA"   , "1.Qg8+ Rxg8 2.Nf7#",   12.0  },   // White gives mate
        { "AHB"   , "1.Qg8+ Rxg8 2.Nxg8",   -8.0  },   // Black has large material plus
        { "B"     , "1.Qa1",                0.0   },
        { "BG"    , "1.Qa1 Rc6",            0.0   },
        { "BGA"   , "1.Qa1 Rc6 2.Nf7+",     0.0   },   // equal(ish)
        { "BGB"   , "1.Qa1 Rc6 2.Ng4",      0.0   },   // equal(ish)
        { "BH"    , "1.Qa1 Ng8",            0.0   },
        { "BHA"   , "1.Qa1 Ng8 2.Nf7#",     12.0  },   // White gives mate
        { "BHB"   , "1.Qa1 Ng8 2.Ng4",      0.0   }    // equal(ish)
    }
};

// White can give defend or give Philidor's mate (same as above, with
//  first move reversed)
static Model model3 =
{
    "1rr4k/4n1pp/7N/8/8/8/Q4PPP/6K1 w - - 0 1",
    "White can defend or give Philidor's mate (same as above, with first move reversed)",
    "Since Qg8+ is not first choice, there's less alpha-beta cutoffs than example 2",
    {
        { "A"     , "1.Qa1",                0.0   },
        { "AG"    , "1.Qa1 Rc6",            0.0   },
        { "AGA"   , "1.Qa1 Rc6 2.Nf7+",     0.0   },   // equal(ish)
        { "AGB"   , "1.Qa1 Rc6 2.Ng4",      0.0   },   // equal(ish)
        { "AH"    , "1.Qa1 Ng8",            0.0   },
        { "AHA"   , "1.Qa1 Ng8 2.Nf7#",     12.0  },   // White gives mate
        { "AHB"   , "1.Qa1 Ng8 2.Ng4",      0.0   },   // equal(ish)
        { "B"     , "1.Qg8+",               0.0   },
        { "BG"    , "1.Qg8+ Nxg8",          0.0   },
        { "BGA"   , "1.Qg8+ Nxg8 2.Nf7#",   12.0  },   // White gives mate
        { "BGB"   , "1.Qg8+ Nxg8 2.Nxg8",   -10.0 },   // Black has huge material plus
        { "BH"    , "1.Qg8+ Rxg8",          0.0   },
        { "BHA"   , "1.Qg8+ Rxg8 2.Nf7#",   12.0  },   // White gives mate
        { "BHB"   , "1.Qg8+ Rxg8 2.Nxg8",   -8.0  }    // Black has large material plus
    }
};

// White can win a rook, or give mate in some lines
static Model model4 =
{
    "8/r5kp/6pr/8/1n1N4/6R1/6PP/3R3K w - - 0 1",
    "White can win a rook, or give mate in some lines",
    "Decision 4) is a good example of Alpha-Beta cutoff. 2.Rd8 mate refutes\n"
    "2...Kh8, given that 2...Kg8 is not mated. So no need to look at other\n"
    "replies to 2...Kh8\n",
    {
        { "A"     , "1.Nf5+",               0.0  },
        { "AG"    , "1.Nf5+ Kg8",           0.0  },
        { "AGA"   , "1.Nf5+ Kg8 2.Nxh6+",   5.1  },    // White wins a rook
        { "AGB"   , "1.Nf5+ Kg8 2.h3",      0.1  },    // equal(ish)
        { "AH"    , "1.Nf5+ Kh8",           0.0  },
        { "AHA"   , "1.Nf5+ Kh8 2.Rd8#",    12.0 },    // White gives mate
        { "AHB"   , "1.Nf5+ Kh8 2.Nxh6",    5.0  },    // White wins a rook
        { "B"     , "1.Ne6+",               0.0  },
        { "BG"    , "1.Ne6+ Kg8",           0.0  },
        { "BGA"   , "1.Ne6+ Kg8 2.h3",      0.3  },    // equal(ish)
        { "BGB"   , "1.Ne6+ Kg8 2.Rd8+",    0.5  },    // equal(ish)
        { "BH"    , "1.Ne6+ Kh8",           0.0  },
        { "BHA"   , "1.Ne6+ Kh8 2.h3",      0.2  },    // equal(ish)
        { "BHB"   , "1.Ne6+ Kh8 2.Rd8#",    12.0 }     // White gives mate
    }
};

// White can take a bishop or fork king queen and rook
static Model model5 =
{
   "7k/R1ppp1p1/1p6/4q3/5N1r/3b3P/PP3PP1/Q5K1 w - - 0 1",
   "This is the same as Example 1) except the static score for move B 1.Ng6+\n"
   "is 1.0 (versus 0.0 for move A 1.Nxd3). Static scores for non-leaf nodes\n"
   "don't affect the ultimate PV calculated, but they do result in\n"
   "re-ordering of evaluations. The result here is that branch B is\n"
   "evaluated first, so this time there are Alpha-Beta cutoffs. Alpha-Beta\n"
   "works best when stronger moves are evaluated first.\n",
   "So the result is an optimised calculation compared to Example 1)",
    {
       { "A"     , "1.Nxd3",                0.0  },
       { "AG"    , "1.Nxd3 Qd6",            0.0  },
       { "AGA"   , "1.Nxd3 Qd6 2.Ne1",      3.3  },    // White wins a bishop
       { "AGB"   , "1.Nxd3 Qd6 2.Qb1",      3.0  },    // White wins a bishop
       { "AH"    , "1.Nxd3 Qg5",            0.0  },
       { "AHA"   , "1.Nxd3 Qg5 2.Rxc7",     3.1  },    // White wins a bishop
       { "AHB"   , "1.Nxd3 Qg5 2.Kh2",      3.2  },    // White wins a bishop
       { "B"     , "1.Ng6+",                1.0  },
       { "BG"    , "1.Ng6+ Kh7",            0.0  },
       { "BGA"   , "1.Ng6+ Kh7 2.Nxe5",     9.2  },    // White wins a queen
       { "BGB"   , "1.Ng6+ Kh7 2.Nxh4",     5.0  },    // White wins a rook
       { "BH"    , "1.Ng6+ Kg8",            0.0  },
       { "BHA"   , "1.Ng6+ Kg8 2.Nxe5",     9.0  },    // White wins a queen
       { "BHB"   , "1.Ng6+ Kg8 2.Nxh4",     5.2  }     // White wins a rook
    }
};

// Consider moving this simple string utility to util.cpp
static std::string overwrite_before_offset( const std::string &s, size_t offset, const std::string &insert )
{
    std::string ret;
    if( insert.length() <= offset )
    {
        ret =  s.substr( 0, offset - insert.length() );
        ret += insert;
        ret += s.substr( offset );
    }
    else
    {
        ret =  insert.substr( insert.length() - offset );
        ret += s.substr( offset );
    }
    return ret;    
}

// Consider moving this simple string utility to util.cpp
static std::string overwrite_at_offset( const std::string &s, size_t offset, const std::string &insert )
{
    std::string ret;
    ret = s.substr( 0, offset );
    ret += insert;
    if( offset + insert.length() <= s.length() )
        ret += s.substr( offset + insert.length() );
    return ret;    
}

// Callback needs access to the running example
static Example *running_example;

// Use a simple example to explore/probe the minimax algorithm and verify it
void sargon_minimax_main()
{
    // Print big picture graphics
    for( std::string s: big_picture )
        printf( "%s\n", s.c_str() );

    // Print explanation of the annotation
    printf( "\nIn the examples below the lines are annotated as follows;\n"
        "Moves (N) [A,M] Score Result\n"
        " N indicates the evaluation order,\n"
        " A is the Alpha-Beta threshold (two ply down),\n"
        " M is the minimax threshold (one ply down),\n"
        " Score is the node score (static score for leaf nodes, minimax score for\n"
        "  non-leaf nodes),\n"
        " Result indicates the result of comparing the score to the thresholds\n" );

    // Loop through multiple examples
    int example_nbr = 1;
    std::vector<Model *> models = {&model1,&model2,&model3,&model4,&model5};
    for( Model *model: models )
    {
        Example example(*model);
        running_example = &example;
        example.Run();
        example.PrintIntro( example_nbr++ );

        // Annotate lines with progress through minimax calculation
        example.Annotate();

        //Run PV algorithm, asterisk the PV nodes
        example.CalculatePV();

        // Print ascii-art
        example.PrintDiagram();

        // Print conclusion
        example.PrintConclusion();
    }
}

// For regression testing, check just check that we get expected detailed log
std::string model1_detailed_log =
"Position 0, \"\" created in tree\n"
"Position 1, \"1.Nxd3\" created in tree\n"
"Position 2, \"1.Ng6+\" created in tree\n"
"Position 3, \"1.Nxd3 Qd6\" created in tree\n"
"Position 4, \"1.Nxd3 Qg5\" created in tree\n"
"Position 5, \"1.Nxd3 Qd6 2.Ne1\" created in tree\n"
"Eval (ply 3), 1.Nxd3 Qd6 2.Ne1\n"
"No alpha beta cutoff because move value=3.3 < two lower ply value=MAX\n"
"Best move because negated move value=-3.3 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 6, \"1.Nxd3 Qd6 2.Qb1\" created in tree\n"
"Eval (ply 3), 1.Nxd3 Qd6 2.Qb1\n"
"No alpha beta cutoff because move value=3.0 < two lower ply value=MAX\n"
"Not best move because negated move value=-3.0 >= one lower ply value=-3.3\n"
"Eval (ply 2), 1.Nxd3 Qd6\n"
"No alpha beta cutoff because move value=-3.3 < two lower ply value=MAX\n"
"Best move because negated move value=3.3 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 7, \"1.Nxd3 Qg5 2.Rxc7\" created in tree\n"
"Eval (ply 3), 1.Nxd3 Qg5 2.Rxc7\n"
"No alpha beta cutoff because move value=3.1 < two lower ply value=3.3\n"
"Best move because negated move value=-3.1 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 8, \"1.Nxd3 Qg5 2.Kh2\" created in tree\n"
"Eval (ply 3), 1.Nxd3 Qg5 2.Kh2\n"
"No alpha beta cutoff because move value=3.2 < two lower ply value=3.3\n"
"Best move because negated move value=-3.2 < one lower ply value=-3.1\n"
"(Confirming best move)\n"
"Eval (ply 2), 1.Nxd3 Qg5\n"
"No alpha beta cutoff because move value=-3.2 < two lower ply value=MAX\n"
"Best move because negated move value=3.2 < one lower ply value=3.3\n"
"(Confirming best move)\n"
"Eval (ply 1), 1.Nxd3\n"
"No alpha beta cutoff because move value=3.2 < two lower ply value=MAX\n"
"Best move because negated move value=-3.2 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 9, \"1.Ng6+ Kh7\" created in tree\n"
"Position 10, \"1.Ng6+ Kg8\" created in tree\n"
"Position 11, \"1.Ng6+ Kh7 2.Nxe5\" created in tree\n"
"Eval (ply 3), 1.Ng6+ Kh7 2.Nxe5\n"
"No alpha beta cutoff because move value=9.2 < two lower ply value=MAX\n"
"Best move because negated move value=-9.2 < one lower ply value=-3.2\n"
"(Confirming best move)\n"
"Position 12, \"1.Ng6+ Kh7 2.Nxh4\" created in tree\n"
"Eval (ply 3), 1.Ng6+ Kh7 2.Nxh4\n"
"No alpha beta cutoff because move value=5.0 < two lower ply value=MAX\n"
"Not best move because negated move value=-5.0 >= one lower ply value=-9.2\n"
"Eval (ply 2), 1.Ng6+ Kh7\n"
"No alpha beta cutoff because move value=-9.2 < two lower ply value=-3.2\n"
"Best move because negated move value=9.2 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 13, \"1.Ng6+ Kg8 2.Nxe5\" created in tree\n"
"Eval (ply 3), 1.Ng6+ Kg8 2.Nxe5\n"
"No alpha beta cutoff because move value=9.0 < two lower ply value=9.2\n"
"Best move because negated move value=-9.0 < one lower ply value=-3.2\n"
"(Confirming best move)\n"
"Position 14, \"1.Ng6+ Kg8 2.Nxh4\" created in tree\n"
"Eval (ply 3), 1.Ng6+ Kg8 2.Nxh4\n"
"No alpha beta cutoff because move value=5.2 < two lower ply value=9.2\n"
"Not best move because negated move value=-5.2 >= one lower ply value=-9.0\n"
"Eval (ply 2), 1.Ng6+ Kg8\n"
"No alpha beta cutoff because move value=-9.0 < two lower ply value=-3.2\n"
"Best move because negated move value=9.0 < one lower ply value=9.2\n"
"(Confirming best move)\n"
"Eval (ply 1), 1.Ng6+\n"
"No alpha beta cutoff because move value=9.0 < two lower ply value=MAX\n"
"Best move because negated move value=-9.0 < one lower ply value=-3.2\n"
"(Confirming best move)\n";

static std::string model2_detailed_log =
"Position 0, \"\" created in tree\n"
"Position 1, \"1.Qg8+\" created in tree\n"
"Position 2, \"1.Qa1\" created in tree\n"
"Position 3, \"1.Qg8+ Nxg8\" created in tree\n"
"Position 4, \"1.Qg8+ Rxg8\" created in tree\n"
"Position 5, \"1.Qg8+ Nxg8 2.Nf7#\" created in tree\n"
"Eval (ply 3), 1.Qg8+ Nxg8 2.Nf7#\n"
"No alpha beta cutoff because move value=12.0 < two lower ply value=MAX\n"
"Best move because negated move value=-12.0 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 6, \"1.Qg8+ Nxg8 2.Nxg8\" created in tree\n"
"Eval (ply 3), 1.Qg8+ Nxg8 2.Nxg8\n"
"No alpha beta cutoff because move value=-10.0 < two lower ply value=MAX\n"
"Not best move because negated move value=10.0 >= one lower ply value=-12.0\n"
"Eval (ply 2), 1.Qg8+ Nxg8\n"
"No alpha beta cutoff because move value=-12.0 < two lower ply value=MAX\n"
"Best move because negated move value=12.0 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 7, \"1.Qg8+ Rxg8 2.Nf7#\" created in tree\n"
"Eval (ply 3), 1.Qg8+ Rxg8 2.Nf7#\n"
"Alpha beta cutoff because move value=12.0 >= two lower ply value=12.0\n"
"Eval (ply 1), 1.Qg8+\n"
"No alpha beta cutoff because move value=12.0 < two lower ply value=MAX\n"
"Best move because negated move value=-12.0 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 9, \"1.Qa1 Rc6\" created in tree\n"
"Position 10, \"1.Qa1 Ng8\" created in tree\n"
"Position 11, \"1.Qa1 Rc6 2.Nf7+\" created in tree\n"
"Eval (ply 3), 1.Qa1 Rc6 2.Nf7+\n"
"No alpha beta cutoff because move value=0.0 < two lower ply value=MAX\n"
"Not best move because negated move value=0.0 >= one lower ply value=-12.0\n"
"Position 12, \"1.Qa1 Rc6 2.Ng4\" created in tree\n"
"Eval (ply 3), 1.Qa1 Rc6 2.Ng4\n"
"No alpha beta cutoff because move value=0.0 < two lower ply value=MAX\n"
"Not best move because negated move value=0.0 >= one lower ply value=-12.0\n"
"Eval (ply 2), 1.Qa1 Rc6\n"
"Alpha beta cutoff because move value=-12.0 >= two lower ply value=-12.0\n";

std::string model3_detailed_log =
"Position 0, \"\" created in tree\n"
"Position 1, \"1.Qa1\" created in tree\n"
"Position 2, \"1.Qg8+\" created in tree\n"
"Position 3, \"1.Qa1 Rc6\" created in tree\n"
"Position 4, \"1.Qa1 Ng8\" created in tree\n"
"Position 5, \"1.Qa1 Rc6 2.Nf7+\" created in tree\n"
"Eval (ply 3), 1.Qa1 Rc6 2.Nf7+\n"
"No alpha beta cutoff because move value=0.0 < two lower ply value=MAX\n"
"Best move because negated move value=0.0 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 6, \"1.Qa1 Rc6 2.Ng4\" created in tree\n"
"Eval (ply 3), 1.Qa1 Rc6 2.Ng4\n"
"No alpha beta cutoff because move value=0.0 < two lower ply value=MAX\n"
"Not best move because negated move value=0.0 >= one lower ply value=0.0\n"
"Eval (ply 2), 1.Qa1 Rc6\n"
"No alpha beta cutoff because move value=0.0 < two lower ply value=MAX\n"
"Best move because negated move value=0.0 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 7, \"1.Qa1 Ng8 2.Nf7#\" created in tree\n"
"Eval (ply 3), 1.Qa1 Ng8 2.Nf7#\n"
"Alpha beta cutoff because move value=12.0 >= two lower ply value=0.0\n"
"Eval (ply 1), 1.Qa1\n"
"No alpha beta cutoff because move value=0.0 < two lower ply value=MAX\n"
"Best move because negated move value=0.0 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 9, \"1.Qg8+ Nxg8\" created in tree\n"
"Position 10, \"1.Qg8+ Rxg8\" created in tree\n"
"Position 11, \"1.Qg8+ Nxg8 2.Nf7#\" created in tree\n"
"Eval (ply 3), 1.Qg8+ Nxg8 2.Nf7#\n"
"No alpha beta cutoff because move value=12.0 < two lower ply value=MAX\n"
"Best move because negated move value=-12.0 < one lower ply value=0.0\n"
"(Confirming best move)\n"
"Position 12, \"1.Qg8+ Nxg8 2.Nxg8\" created in tree\n"
"Eval (ply 3), 1.Qg8+ Nxg8 2.Nxg8\n"
"No alpha beta cutoff because move value=-10.0 < two lower ply value=MAX\n"
"Not best move because negated move value=10.0 >= one lower ply value=-12.0\n"
"Eval (ply 2), 1.Qg8+ Nxg8\n"
"No alpha beta cutoff because move value=-12.0 < two lower ply value=0.0\n"
"Best move because negated move value=12.0 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 13, \"1.Qg8+ Rxg8 2.Nf7#\" created in tree\n"
"Eval (ply 3), 1.Qg8+ Rxg8 2.Nf7#\n"
"Alpha beta cutoff because move value=12.0 >= two lower ply value=12.0\n"
"Eval (ply 1), 1.Qg8+\n"
"No alpha beta cutoff because move value=12.0 < two lower ply value=MAX\n"
"Best move because negated move value=-12.0 < one lower ply value=0.0\n"
"(Confirming best move)\n";

std::string model4_detailed_log =
"Position 0, \"\" created in tree\n"
"Position 1, \"1.Nf5+\" created in tree\n"
"Position 2, \"1.Ne6+\" created in tree\n"
"Position 3, \"1.Nf5+ Kg8\" created in tree\n"
"Position 4, \"1.Nf5+ Kh8\" created in tree\n"
"Position 5, \"1.Nf5+ Kg8 2.Nxh6+\" created in tree\n"
"Eval (ply 3), 1.Nf5+ Kg8 2.Nxh6+\n"
"No alpha beta cutoff because move value=5.1 < two lower ply value=MAX\n"
"Best move because negated move value=-5.1 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 6, \"1.Nf5+ Kg8 2.h3\" created in tree\n"
"Eval (ply 3), 1.Nf5+ Kg8 2.h3\n"
"No alpha beta cutoff because move value=0.1 < two lower ply value=MAX\n"
"Not best move because negated move value=-0.1 >= one lower ply value=-5.1\n"
"Eval (ply 2), 1.Nf5+ Kg8\n"
"No alpha beta cutoff because move value=-5.1 < two lower ply value=MAX\n"
"Best move because negated move value=5.1 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 7, \"1.Nf5+ Kh8 2.Rd8#\" created in tree\n"
"Eval (ply 3), 1.Nf5+ Kh8 2.Rd8#\n"
"Alpha beta cutoff because move value=12.0 >= two lower ply value=5.1\n"
"Eval (ply 1), 1.Nf5+\n"
"No alpha beta cutoff because move value=5.1 < two lower ply value=MAX\n"
"Best move because negated move value=-5.1 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 9, \"1.Ne6+ Kg8\" created in tree\n"
"Position 10, \"1.Ne6+ Kh8\" created in tree\n"
"Position 11, \"1.Ne6+ Kg8 2.h3\" created in tree\n"
"Eval (ply 3), 1.Ne6+ Kg8 2.h3\n"
"No alpha beta cutoff because move value=0.3 < two lower ply value=MAX\n"
"Not best move because negated move value=-0.3 >= one lower ply value=-5.1\n"
"Position 12, \"1.Ne6+ Kg8 2.Rd8+\" created in tree\n"
"Eval (ply 3), 1.Ne6+ Kg8 2.Rd8+\n"
"No alpha beta cutoff because move value=0.5 < two lower ply value=MAX\n"
"Not best move because negated move value=-0.5 >= one lower ply value=-5.1\n"
"Eval (ply 2), 1.Ne6+ Kg8\n"
"Alpha beta cutoff because move value=-5.1 >= two lower ply value=-5.1\n";

std::string model5_detailed_log =
"Position 0, \"\" created in tree\n"
"Position 1, \"1.Nxd3\" created in tree\n"
"Position 2, \"1.Ng6+\" created in tree\n"
"Position 9, \"1.Ng6+ Kh7\" created in tree\n"
"Position 10, \"1.Ng6+ Kg8\" created in tree\n"
"Position 11, \"1.Ng6+ Kh7 2.Nxe5\" created in tree\n"
"Eval (ply 3), 1.Ng6+ Kh7 2.Nxe5\n"
"No alpha beta cutoff because move value=9.2 < two lower ply value=MAX\n"
"Best move because negated move value=-9.2 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 12, \"1.Ng6+ Kh7 2.Nxh4\" created in tree\n"
"Eval (ply 3), 1.Ng6+ Kh7 2.Nxh4\n"
"No alpha beta cutoff because move value=5.0 < two lower ply value=MAX\n"
"Not best move because negated move value=-5.0 >= one lower ply value=-9.2\n"
"Eval (ply 2), 1.Ng6+ Kh7\n"
"No alpha beta cutoff because move value=-9.2 < two lower ply value=MAX\n"
"Best move because negated move value=9.2 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 13, \"1.Ng6+ Kg8 2.Nxe5\" created in tree\n"
"Eval (ply 3), 1.Ng6+ Kg8 2.Nxe5\n"
"No alpha beta cutoff because move value=9.0 < two lower ply value=9.2\n"
"Best move because negated move value=-9.0 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 14, \"1.Ng6+ Kg8 2.Nxh4\" created in tree\n"
"Eval (ply 3), 1.Ng6+ Kg8 2.Nxh4\n"
"No alpha beta cutoff because move value=5.2 < two lower ply value=9.2\n"
"Not best move because negated move value=-5.2 >= one lower ply value=-9.0\n"
"Eval (ply 2), 1.Ng6+ Kg8\n"
"No alpha beta cutoff because move value=-9.0 < two lower ply value=MAX\n"
"Best move because negated move value=9.0 < one lower ply value=9.2\n"
"(Confirming best move)\n"
"Eval (ply 1), 1.Ng6+\n"
"No alpha beta cutoff because move value=9.0 < two lower ply value=MAX\n"
"Best move because negated move value=-9.0 < one lower ply value=MAX\n"
"(Confirming best move)\n"
"Position 3, \"1.Nxd3 Qd6\" created in tree\n"
"Position 4, \"1.Nxd3 Qg5\" created in tree\n"
"Position 5, \"1.Nxd3 Qd6 2.Ne1\" created in tree\n"
"Eval (ply 3), 1.Nxd3 Qd6 2.Ne1\n"
"No alpha beta cutoff because move value=3.3 < two lower ply value=MAX\n"
"Not best move because negated move value=-3.3 >= one lower ply value=-9.0\n"
"Position 6, \"1.Nxd3 Qd6 2.Qb1\" created in tree\n"
"Eval (ply 3), 1.Nxd3 Qd6 2.Qb1\n"
"No alpha beta cutoff because move value=3.0 < two lower ply value=MAX\n"
"Not best move because negated move value=-3.0 >= one lower ply value=-9.0\n"
"Eval (ply 2), 1.Nxd3 Qd6\n"
"Alpha beta cutoff because move value=-9.0 >= two lower ply value=-9.0\n";

// Regression test
bool sargon_minimax_regression_test( bool quiet)
{
    bool ok = true;

    // Loop through multiple examples
    int example_nbr = 1;
    std::vector<Model *> models = {&model1,&model2,&model3,&model4,&model5};
    for( Model *model: models )
    {
        Example example(*model);
        running_example = &example;
        example.Run();
        std::string s = example.DetailedLog();
        std::string t;
        switch( example_nbr )
        {
            case 1: t = model1_detailed_log; break;
            case 2: t = model2_detailed_log; break;
            case 3: t = model3_detailed_log; break;
            case 4: t = model4_detailed_log; break;
            case 5: t = model5_detailed_log; break;
        }
        bool pass = (s==t);
        std::string result = util::sprintf( "%s model%d: %s", pass?"PASS":"FAIL", example_nbr, model->comment1.c_str() );
        if( result.length() > 79 )
            result = result.substr(0,75) + "...";
        printf( "%s\n", result.c_str() );
        if( !pass )
        {
            ok = false;
            if( !quiet )
            {
                printf( "Expected:\n" );
                printf( "%s\n", t.c_str() );
                printf( "Actual:\n" );
                printf( "%s\n", s.c_str() );
            }
        }
        example_nbr++;
    }
    return ok;
}

// Sargon calls back into this function as it runs, we can monitor what's going on by
//  reading registers and peeking at memory, and influence it by modifying registers
//  and poking at memory.
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
        if( !callback_minimax_mods_active )
            return;

        // For purposes of minimax tracing experiment, we only want two possible
        //  moves in each position - achieved by suppressing King moves
        if( std::string(msg) == "Suppress King moves" )
        {
            unsigned char piece = peekb(T1);
            if( piece == 6 )    // King?
            {
                // Change al to 2 and ch to 1 and MPIECE will exit without
                //  generating (non-castling) king moves
                volatile uint32_t *peax = &reg_eax;   // note use of volatile keyword
                *peax = 2;                            // MODIFY VALUE !
                volatile uint32_t *pecx = &reg_ecx;   // note use of volatile keyword
                *pecx = 0x100;                        // MODIFY VALUE !
            }
        }

        // For purposes of minimax tracing experiment, we inject our own points
        //  score for each known position (we keep the number of positions to
        //  managable levels.)
        else if( std::string(msg) == "end of POINTS()" )
        {
            std::string key = get_key();
            Progress prog;
            prog.pt  = create;
            prog.key = key;
            prog.msg = util::sprintf( "Position %d, \"%s\" created in tree",
                                            running_example->cardinal_nbr[key],
                                            running_example->lines[key].c_str() );
            running_example->progress.push_back(prog);
            unsigned int value = running_example->values[key];
            volatile uint32_t *peax = &reg_eax;     // note use of volatile keyword
            *peax = value;                          // MODIFY VALUE !
        }

        // For purposes of minimax tracing experiment, describe and annotate the
        //  best move calculation
        else if( std::string(msg) == "Alpha beta cutoff?" )
        {
            Progress prog;
            std::string key = get_key();

            // Eval takes place after undoing last move, so need to add it back to
            //  show position meaningfully
            unsigned int  p     = peekw(MLPTRJ);
            unsigned char from  = peekb(p+2);
            thc::Square sq;
            sargon_export_square(from,sq);
            char c = thc::get_file(sq);
            if( key == "(root)" )
                key = "";
            key += toupper(c); 
            unsigned int al  = reg_eax&0xff;
            unsigned int bx  = reg_ebx&0xffff;
            unsigned int val = peekb(bx);
            bool jmp = (al <= val);   // Note that Sargon integer values have reverse sense to
                                      //  float centipawns.
                                      //  So jmp if al <= val means
                                      //     jmp if float(al) >= float(val)
            std::string float_value = (val==0 ? "MAX" : util::sprintf("%.1f",sargon_export_value(val)) ); // Show "MAX" instead of "12.8"
            prog.key = key;
            prog.pt  = eval;
            prog.move_val = al;
            prog.alphabeta_compare_val = val;
            prog.minimax_compare_val = peekb(bx+1);
            prog.msg = util::sprintf( "Eval (ply %d), %s", peekb(NPLY), running_example->lines[key].c_str() );
            running_example->progress.push_back(prog);
            if( jmp )   // jmp matches the Sargon assembly code jump decision. Jump if Alpha-Beta cutoff
            {
                prog.pt  = alpha_beta_yes;
                prog.msg = util::sprintf( "Alpha beta cutoff because move value=%.1f >= two lower ply value=%s",
                sargon_export_value(al),
                float_value.c_str() );
                prog.diagram_msg = util::sprintf( ">=%s so ALPHA BETA CUTOFF",
                float_value.c_str() );
            }
            else
            {
                prog.pt  = alpha_beta_no;
                prog.msg = util::sprintf( "No alpha beta cutoff because move value=%.1f < two lower ply value=%s",
                sargon_export_value(al),
                float_value.c_str() );
            }
            running_example->progress.push_back(prog);
        }
        else if( std::string(msg) == "No. Best move?" )
        {
            Progress prog;
            unsigned int al  = reg_eax&0xff;
            unsigned int bx  = reg_ebx&0xffff;
            unsigned int val = peekb(bx);
            bool jmp = (al <= val);   // Note that Sargon integer values have reverse sense to
                                      //  float centipawns.
                                      //  So jmp if al <= val means
                                      //     jmp if float(al) >= float(val)
            std::string float_value = (val==0 ? "MAX" : util::sprintf("%.1f",sargon_export_value(val)) ); // Show "MAX" instead of "12.8"
            std::string neg_float_value = (val==0 ? " -MAX" : util::sprintf("%.1f",0.0-sargon_export_value(val)) ); // Show "-MAX" instead of "-12.8"
            if( jmp )   // jmp matches the Sargon assembly code jump decision. Jump if not best move
            {
                prog.pt  = bestmove_no;
                prog.msg = util::sprintf( "Not best move because negated move value=%.1f >= one lower ply value=%s",
                sargon_export_value(al),
                float_value.c_str() );
                prog.diagram_msg = util::sprintf( "<=%s so discard",
                neg_float_value.c_str() );
            }
            else
            {
                prog.pt  = bestmove_yes;
                prog.msg = util::sprintf( "Best move because negated move value=%.1f < one lower ply value=%s",
                sargon_export_value(al),
                float_value.c_str() );
                prog.diagram_msg = util::sprintf( ">%s so NEW BEST MOVE",
                neg_float_value.c_str() );
            }
            running_example->progress.push_back(prog);
        }
        else if( std::string(msg) == "Yes! Best move" )
        {
            Progress prog;
            prog.pt  = bestmove_confirmed;
            prog.msg = "(Confirming best move)";
            running_example->progress.push_back(prog);
        }
    }
};

