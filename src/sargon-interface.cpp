/*

  Sargon interfacing functions
  
  */

#define _CRT_SECURE_NO_WARNINGS
#include "util.h"
#include "thc.h"
#include "sargon-interface.h"
#include "sargon-asm-interface.h"

// Write chess position into Sargon (inner-most part)
static void sargon_import_position_inner( const thc::ChessPosition &cp );

// Value seems to be 128 = 0.0
// 128-30 = 3.0
// 128+30 = -3.0
// 128-128 = 0 = -12.8
double sargon_export_value( unsigned int value )
{
    double m = -3.0 / 30.0;
    double c = 12.8;
    double y = m * value + c;
    return y;
}

unsigned int sargon_import_value( double value )
{
    if( value < -12.6 )
        value = -12.6;
    if( value > 12.6 )
        value = 12.6;
    double m = -10.0;
    double c = 128.0;
    double y = m * value + c;
    return static_cast<unsigned int>(y);
}


struct NODE2
{
    unsigned int level;
    unsigned char from;
    unsigned char to;
    unsigned char flags;
    unsigned char value;
    NODE2() : level(0), from(0), to(0), flags(0), value(0) {}
    NODE2( unsigned int l, unsigned char f, unsigned char t, unsigned char fs, unsigned char v ) : level(l), from(f), to(t), flags(fs), value(v) {}
};

static NODE2 pv[10];
static std::vector< NODE2 > nodes;

// Read a square value out of Sargon
bool sargon_export_square( unsigned int sargon_square, thc::Square &sq )
{
    bool ok=false;
    if( sargon_square >= 21 )
    {
        int rank = (sargon_square-21)/10;               // eg a1=21 ->0, h1=28->0, a2=31->1 ... a8=91 ->7, h8=98 ->7
        int file = sargon_square - (21 + (rank*10));    // eg a1=21 ->0, h1=28->7, a2=31->0 ... a8=91 ->0, h8=98 ->7
        if( 0<=rank && rank<=7 && 0<=file && file<=7 )
        {
            ok = true;
            int offset = (7-rank)*8 + file;             // eg a1->56, h1->63, a2->48 ...  a8->0, h8->7
            sq = static_cast<thc::Square>(offset);
        }
    }
    return ok;
}

// Read a chess move out of Sargon (returns "Terse" form - eg "e1g1" for White O-O, note
//  that Sargon always promotes to Queen, so four character form is sufficient)
std::string sargon_export_move( unsigned int sargon_move_ptr )
{
    char buf[5];
    buf[0] = '\0';
    unsigned int  p    = peekw(sargon_move_ptr);
    unsigned char from = peekb(p+2);
    unsigned char to   = peekb(p+3);
    thc::Square f, t;
    if( sargon_export_square(from,f) )
    {
        if( sargon_export_square(to,t) )
        {
            buf[0] = thc::get_file(f);
            buf[1] = thc::get_rank(f);
            buf[2] = thc::get_file(t);
            buf[3] = thc::get_rank(t);
            buf[4] = '\0';
        }
    }
    return std::string(buf);
}

// Play a move inside Sargon (i.e. update Sargon's representation with a legal
//  played move)
bool sargon_play_move( thc::Move &mv )
{

    // This function is modelled on PLYRMV() Sargon user interface
    //  function

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

    */

    bool ok=false;
    unsigned char color = peekb(COLOR); // who to move?
    unsigned char kolor = peekb(KOLOR); // which side is Sargon?
    pokeb( KOLOR, color==0?0x80:0 );    // VALMOV validates a move against Sargon, so Sargon must be the opposite to the side to move
    std::string terse = mv.TerseOut();
    z80_registers regs;
    for( int i=0; i<2; i++ )
    {
        memset( &regs, 0, sizeof(regs) );
        unsigned int file = terse[0 + 2*i] - 0x20;    // toupper for ASNTBI()
        unsigned int rank = terse[1 + 2*i];
        regs.hl = (file<<8) + rank;     // eg set hl registers = 0x4838 = "H8"
        sargon( api_ASNTBI, &regs );    // ASNTBI = ASCII square name to board index
        ok = (((regs.bc>>8) & 0xff) == 0);  // ok if reg B eq 0
        if( ok )
        {
            unsigned int board_index = (regs.af&0xff);  // A register is low part of regs.af
            pokeb(MVEMSG+i,board_index);
        }
    }
    if( ok )
    {
        sargon( api_VALMOV, &regs );
        ok = ((regs.af & 0xff) == 0);  // ok if reg A eq 0
        if( ok )
            sargon( api_EXECMV, &regs );
    }

    // Restore COLOR and KOLOR
    pokeb( KOLOR, kolor );
    if( ok )
        pokeb( COLOR, color==0?0x80:0 );  // toggle side to move
    else
        pokeb( COLOR, color );
    return ok;
}

// Read chess position from Sargon
void sargon_export_position( thc::ChessPosition &cp )
{
    cp.Init();
    const unsigned char *sargon_board = peek(BOARDA);
    const unsigned char *src_base = sargon_board + 28;  // square h1
    char *dst_base = &cp.squares[thc::h1];
    for( int i=0; i<8; i++ )
    {
        const unsigned char *src = src_base + i*10;
        char *dst = dst_base - i*8;
        for( int j=0; j<8; j++ )
        {
            unsigned char b = *src--;
            char c = ' ';
            b &= 0x87;
            switch( b )
            {
                case 0x81:   c = 'p';   break;
                case 0x82:   c = 'n';   break;
                case 0x83:   c = 'b';   break;
                case 0x84:   c = 'r';   break;
                case 0x85:   c = 'q';   break;
                case 0x86:   c = 'k';   break;
                case 0x01:   c = 'P';   break;
                case 0x02:   c = 'N';   break;
                case 0x03:   c = 'B';   break;
                case 0x04:   c = 'R';   break;
                case 0x05:   c = 'Q';   break;
                case 0x06:   c = 'K';   break;
            }
            *dst-- = c;
        }
    }
    cp.white = (peekb(COLOR) == 0x00);
    cp.full_move_count = peekb(MOVENO);
}

// Write chess position into Sargon
void sargon_import_position( const thc::ChessPosition &cp )
{
    // Sargon's move evaluation takes some account of the full move number (it
    //  prioritises moving unmoved pieces early). So get an approximation to
    //  that by counting pieces that aren't in their initial positions
    int moveno = 0;
    thc::ChessRules init;
    thc::ChessPosition cp_work = cp;

    // Get number of moved white and black pieces
    int black_count=0, white_count=0;
    for( int i=0; i<16; i++ )
    {
        if( init.squares[i] != cp_work.squares[i] )
            black_count++;
    }
    for( int i=48; i<64; i++ )
    {
        if( init.squares[i] != cp_work.squares[i] )
            white_count++;
    }

    // Check initial and one half move played positions, set moveno = 1 for
    //  those cases only to get Book move
    if( cp_work.WhiteToPlay() && white_count==0 )
    {
        if( cp_work.ToDebugStr() == init.ToDebugStr() )
            moveno = 1;
    }
    else if( !cp_work.WhiteToPlay() && black_count==0 )
    {
        std::vector<thc::Move> moves;
        init.GenLegalMoveList( moves );
        for( thc::Move mv: moves )
        {
            init.PushMove(mv);
            if( cp_work.ToDebugStr() == init.ToDebugStr() )
                moveno = 1;
            init.PopMove(mv);
        }
    }

    // Otherwise average number of moved black and white pieces, plus a
    //  little to account for pieces moved twice etc. and to compensate
    //  for otherwise undercounting in mid length games (eg 40 move games)
    //  and finally to avoid moveno == 1 unless it is one of the 21 known
    //  positions after 0 or 1 half moves (since moveno==1 generates a
    //  book move)
    if( moveno == 0 )
    {
        moveno = (black_count+white_count) / 2;
        moveno += 2;
    }
    cp_work.full_move_count = moveno;

    // To support en-passant, create position before double pawn advance
    //  then play double pawn advance
    thc::Square sq = cp.enpassant_target;
    if( sq == thc::SQUARE_INVALID )
    {
        sargon_import_position_inner( cp_work );
    }
    else
    {
        int isq = static_cast<int>(sq);
        int from=0, to=0;

        // If en-passant with White to play, en-passant target should be
        //  on 6th rank, eg if e6 undo 'p' e7 -> e5
        if( cp_work.white && thc::get_rank(sq)=='6' && cp_work.squares[isq+8]=='p' )
        {
            cp_work.white = false;
            from = isq - 8;
            to   = isq + 8;
            cp_work.squares[from] = 'p';
            cp_work.squares[to]   = ' ';
        }

        // If en-passant with Black to play, en-passant target should be
        //  on 3rd rank, eg if e3 undo 'P' e2 -> e4
        else if( !cp_work.white && thc::get_rank(sq)=='3' && cp_work.squares[isq-8]=='P' )
        {
            cp_work.white = true;
            from = isq + 8;
            to   = isq - 8;
            cp_work.squares[from] = 'P';
            cp_work.squares[to]   = ' ';
        }
        sargon_import_position_inner( cp_work );
        if( from != to )
        {
            thc::Move mv;
            mv.src = static_cast<thc::Square>(from);
            mv.dst = static_cast<thc::Square>(to);
            mv.capture = ' ';
            mv.special = (cp_work.white ? thc::SPECIAL_WPAWN_2SQUARES : thc::SPECIAL_BPAWN_2SQUARES);
            sargon_play_move( mv );
        }
    }
}

// Write chess position into Sargon (inner-most part)
static void sargon_import_position_inner( const thc::ChessPosition &cp )
{
    pokeb(COLOR,cp.white?0:0x80);
    pokeb(MOVENO,cp.full_move_count);
    static unsigned char board_position[120] =
    {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0x04, 0x02, 0x03, 0x05, 0x06, 0x03, 0x02, 0x04, 0xff,  // <-- White back row, a1 at left end, h1 at right end
        0xff, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xff,  // <-- White pawns
        0xff, 0xa3, 0xb3, 0xc3, 0xd3, 0xe3, 0xf3, 0x00, 0x00, 0xff,  // <-- note 'a3' etc just documents square convention
        0xff, 0xa4, 0xb4, 0xc4, 0xd4, 0xe4, 0xf4, 0x00, 0x00, 0xff,  // <-- but g and h files are 0x00 = empty
        0xff, 0xa5, 0xb5, 0xc5, 0xd5, 0xe5, 0xf5, 0x00, 0x00, 0xff,
        0xff, 0xa6, 0xb6, 0xc6, 0xd6, 0xe6, 0xf6, 0x00, 0x00, 0xff,
        0xff, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xff,  // <-- Black pawns
        0xff, 0x84, 0x82, 0x83, 0x85, 0x86, 0x83, 0x82, 0x84, 0xff,  // <-- Black back row
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };

    memset( board_position, 0xff, sizeof(board_position) );
    unsigned char *dst_base = board_position + 28;  // square h1
    const char *src_base = &cp.squares[thc::h1];
    bool white_king_on_e1 = (cp.squares[thc::e1]=='K');
    bool black_king_on_e8 = (cp.squares[thc::e8]=='k');
    for( int i=0; i<8; i++ )
    {
        unsigned char *dst = dst_base + i*10;
        const char *src = src_base - i*8;
        for( int j=0; j<8; j++ )
        {
            char c = *src--;
            unsigned char b = 0;
            switch( c )
            {
                case 'p':   b = 0x81;   break;
                case 'n':   b = 0x82;   break;
                case 'b':   b = 0x83;   break;
                case 'r':   b = 0x84;   break;
                case 'q':   b = 0x85;   break;
                case 'k':   b = 0x96;   break;
                case 'P':   b = 0x01;   break;
                case 'N':   b = 0x02;   break;
                case 'B':   b = 0x03;   break;
                case 'R':   b = 0x04;   break;
                case 'Q':   b = 0x05;   break;
                case 'K':   b = 0x16;   break;
            }
            bool moved=true;
            if( i==0 )
            {
                if( j==0 && c=='R' && (cp.wking_allowed()||!white_king_on_e1) )
                    moved = false;  // indicate R on home square has not moved, unless it's
                                    //  essential to mark it moved in order to stop illegal castling 
                if( (j==1||j==6) && c=='N' )
                    moved = false; 
                if( (j==2||j==5) && c=='B' )
                    moved = false; 
                if( j==3 && c=='K' && (cp.wking_allowed()||cp.wqueen_allowed()) )
                {
                    moved = false; 
                    b = 0x06;  // uncastled white king
                }
                if( j==4 && c=='Q' )
                    moved = false; 
                if( j==7 && c=='R' && (cp.wqueen_allowed()||!white_king_on_e1)  )
                    moved = false; 
            }
            else if( i==1 && c=='P' )
                moved = false; 
            else if( i==6 && c=='p' )
                moved = false; 
            else if( i==7 )
            {
                if( j==0 && c=='r' && (cp.bking_allowed()||!black_king_on_e8)  )
                    moved = false; 
                if( (j==1||j==6) && c=='n' )
                    moved = false; 
                if( (j==2||j==5) && c=='b' )
                    moved = false; 
                if( j==3 && c=='k' && (cp.bking_allowed()||cp.bqueen_allowed()) )
                {
                    moved = false; 
                    b = 0x86;  // uncastled black king
                }
                if( j==4 && c=='q' )
                    moved = false; 
                if( j==7 && c=='r' && (cp.bqueen_allowed()||!black_king_on_e8) )
                    moved = false; 
            }
            if( moved && b!=0 )
                b += 8;
            *dst-- = b;
        }
    }
    memcpy( poke(BOARDA), board_position, sizeof(board_position) );
}

const unsigned char *peek(int offset)
{
    unsigned char *sargon_mem_base = &sargon_base_address;
    unsigned char *addr = sargon_mem_base + offset;
    return addr;
}

unsigned char peekb(int offset)
{
    const unsigned char *addr = peek(offset);
    return *addr;
}

unsigned int peekw(int offset)
{
    const unsigned char *addr = peek(offset);
    const unsigned char lo = *addr++;
    const unsigned char hi = *addr++;
    unsigned int ret = hi;
    ret = ret <<8;
    ret += lo;
    return ret;
}

unsigned char *poke(int offset)
{
    unsigned char *sargon_mem_base = &sargon_base_address;
    unsigned char *addr = sargon_mem_base + offset;
    return addr;
}

void pokeb( int offset, unsigned char b )
{
    unsigned char *addr = poke(offset);
    *addr = b;
}

std::string algebraic( unsigned int sq )
{
    std::string s = util::sprintf( "%d ??",sq);
    if( sq >= 21 )
    {
        int rank = (sq-21)/10;     // eg a1=21 =0, h1=28=0, a2=31=1
        int file = sq - (21 + (rank*10));
        if( 0<=rank && rank<=7 && 0<=file && file<=7 )
        {
            s = util::sprintf( "%c%c", 'a'+file, '1'+rank );
        }
    }
    return s;
}

    