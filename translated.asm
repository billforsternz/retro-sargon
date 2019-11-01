;X p19
;**********************************************************
;               SARGON
;
;       Sargon is a computer chess playing program designed
; and coded by Dan and Kathe Spracklen. Copyright 1978. All
; rights reserved. No part of this publication may be
; reproduced without the prior written permission.
;**********************************************************

;**********************************************************
; EQUATES
;**********************************************************
	.686P
	.XMM
	.model	flat

CCIR   MACRO ;todo
       ENDM
PRTBLK MACRO name, len ;todo
       ENDM
CARRET MACRO ;todo
       ENDM

;PAWN    =       1
PAWN     EQU     1
;KNIGHT  =       2
KNIGHT   EQU     2
;BISHOP  =       3
BISHOP   EQU     3
;ROOK    =       4
ROOK     EQU     4
;QUEEN   =       5
QUEEN    EQU     5
;KING    =       6
KING     EQU     6
;WHITE   =       0
WHITE    EQU     0
;BLACK   =       80H
BLACK    EQU     80H
;BPAWN   =       BLACK+PAWN
BPAWN    EQU     BLACK+PAWN

;**********************************************************
; TABLES SECTION
;**********************************************************
;        .DATA
_DATA    SEGMENT
;START:  .BLKB   100h
START    DB 100h DUP (?)
;TBASE:
TBASE    EQU $
;X TBASE must be page aligned, it needs an absolute address
;X of 0XX00H. The CP/M ZASM Assembler has an ORG of 110H.
;X The relative address START+0F0H becomes the absolute
;X address 200H.
;**********************************************************
; DIRECT --     Direction Table.  Used to determine the dir-
;               ection of movement of each piece.
;**********************************************************
;DIRECT: .BYTE   +09,+11,-11,-09
DIRECT   DB +09, +11, -11, -09
;        .BYTE   +10,-10,+01,-01
         DB +10, -10, +01, -01
;        .BYTE   -21,-12,+08,+19
         DB -21, -12, +08, +19
;        .BYTE   +21,+12,-08,-19
         DB +21, +12, -08, -19
;        .BYTE   +10,+10,+11,+09
         DB +10, +10, +11, +09
;        .BYTE   -10,-10,-11,-09
         DB -10, -10, -11, -09
;X p20
;**********************************************************
; DPOINT   --   Direction Table Pointer. Used to determine
;               where to begin in the direction table for any
;               given piece.
;**********************************************************
;DPOINT =        .-TBASE
;DPOINT = 18h
DPOINT   EQU     18h

;        .BYTE   20,16,8,0,4,0,0
         DB 20, 16, 8, 0, 4, 0, 0

;**********************************************************
; DCOUNT   --   Direction Table Counter. Used to determine
;               the number of directions of movement for any
;               given piece.
;**********************************************************
;DCOUNT =        .-TBASE
;DCOUNT = 1fh
DCOUNT   EQU     1fh
;        .BYTE   4,4,8,4,4,8,8
         DB 4, 4, 8, 4, 4, 8, 8

;**********************************************************
; PVALUE   --   Point Value. Gives the point value of each
;               piece, or the worth of each piece.
;**********************************************************
;PVALUE =        .-TBASE-1
;PVALUE = 25h
PVALUE   EQU     25h
;        .BYTE   1,3,3,5,9,10
         DB 1, 3, 3, 5, 9, 10

;**********************************************************
; PIECES   --   The initial arrangement of the first rank of
;               pieces on the board. Use to set up the board
;               for the start of the game.
;**********************************************************
;PIECES =        .-TBASE
;PIECES = 2ch
PIECES   EQU     2ch
;        .BYTE   4,2,3,5,6,3,2,4
         DB 4, 2, 3, 5, 6, 3, 2, 4
;X p21
;************************************************************
; BOARD --      Board Array. Used to hold the current position
;               of the board during play. The board itself
;               looks like:
;               FFFFFFFFFFFFFFFFFFFF
;               FFFFFFFFFFFFFFFFFFFF
;               FF0402030506030204FF
;               FF0101010101010101FF
;               FF0000000000000000FF
;               FF0000000000000000FF
;               FF0000000000000060FF
;               FF0000000000000000FF
;               FF8181818181818181FF
;               FF8482838586838284FF
;               FFFFFFFFFFFFFFFFFFFF
;               FFFFFFFFFFFFFFFFFFFF
;               The values of FF form the border of the
;               board, and are used to indicate when a piece
;               moves off the board. The individual bits of
;               the other bytes in the board array are as
;               follows:
;               Bit 7 -- Color of the piece
;                       1 -- Black
;                       0 -- White
;               Bit 6 -- Not used
;               Bit 5 -- Not used
;               Bit 4 --Castle flag for Kings only
;               Bit 3 -- Piece has moved flag
;               Bits 2-0 Piece type
;                       1 -- Pawn
;                       2 -- Knight
;                       3 -- Bishop
;                       4 -- Rook
;                       5 -- Queen
;                       6 -- King
;                       7 -- Not used
;                       0 -- Empty Square
;**********************************************************
;BOARD   =       .-TBASE
;BOARD  = 34h
BOARD    EQU     34h
;BOARDA: .BLKB   120
BOARDA   DB 120 DUP (?)

;p22
;**********************************************************
; ATKLIST --    Attack List. A two part array, the first
;               half for white and the second half for black.
;               It is used to hold the attackers of any given
;               square in the order of their value.
;
; WACT  --      White Attack Count. This is the first
;               byte of the array and tells how many pieces are
;               in the white portion of the attack list.
;
; BACT  --      Black Attack Count. This is the eighth byte of
;               the array and does the same for black.
;**********************************************************
;WACT    =       ATKLST
WACT     EQU     ATKLST
;BACT    =       ATKLST+7
BACT     EQU     ATKLST+7
;ATKLST: .WORD   0,0,0,0,0,0,0
ATKLST   DD 0, 0, 0, 0, 0, 0, 0

;**********************************************************
; PLIST --      Pinned Piece Array. This is a two part array.
;               PLISTA contains the pinned piece position.
;               PLISTD contains the direction from the pinned
;               piece to the attacker.
;**********************************************************
;PLIST   =       .-TBASE-1
;PLIST  = 0c7h
PLIST    EQU     0c7h
;PLISTD  =       PLIST+10
PLISTD   EQU     PLIST+10
;PLISTA: .WORD   0,0,0,0,0,0,0,0,0,0
PLISTA   DD 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

;**********************************************************
; POSK  --      Position of Kings. A two byte area, the first
;               byte of which hold the position of the white
;               king and the second holding the position of
;               the black king.
;
; POSQ  --      Position of Queens. Like POSK,but for queens.
;**********************************************************
;POSK:   .BYTE   24,95
POSK     DB 24, 95
;POSQ:   .BYTE   14,94
POSQ     DB 14, 94
;        .BYTE   -1
         DB -1

;X p23
;**********************************************************
; SCORE --      Score Array. Used during Alpha-Beta pruning to
;               hold the scores at each ply. It includes two
;               "dummy" entries for ply -1 and ply 0.
;**********************************************************
;SCORE:  .WORD   0,0,0,0,0,0
SCORE    DD 0, 0, 0, 0, 0, 0

;**********************************************************
; PLYIX --      Ply Table. Contains pairs of pointers, a pair
;               for each ply. The first pointer points to the
;               top of the list of possible moves at that ply.
;               The second pointer points to which move in the
;               list is the one currently being considered.
;**********************************************************
;PLYIX:  .WORD   0,0,0,0,0,0,0,0,0,0
PLYIX    DD 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
;        .WORD   0,0,0,0,0,0,0,0,0,0
         DD 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

;**********************************************************
; STACK --      Contains the stack for the program.
;**********************************************************
;        .LOC    START+3EFH      ;X START+2FFH
;STACK:
;X Increased stack by 256 bytes. This avoids
;X program crashes on look ahead level 4 and higher.
;Y For this port use standard C stack (if possible)

;X p24
;**********************************************************
; TABLE INDICES SECTION
; M1-M4 --      Working indices used to index into
;               the board array.
; T1-T3 --      Working indices used to index into Direction
;               Count, Direction Value, and Piece Value tables.
; INDX1 --      General working indices. Used for various
; INDX2         purposes.
;
; NPINS --      Number of Pins. Count and pointer into the
;               pinned piece list.
;
; MLPTRI --     Pointer into the ply table which tells
;               which pair of pointers are in current use.
;
; MLPTRJ --     Pointer into the move list to the move that is
;               currently being processed.
;
; SCRIX --      Score Index. Pointer to the score table for
;               the ply being examined.
;
; BESTM --      Pointer into the move list for the move that
;               is currently considered the best by the
;               Alpha-Beta pruning process.
;
; MLLST --      Pointer to the previous move placed in the move
;               list. Used during generation of the move list.
;
; MLNXT --      Pointer to the next available space in the move
;               list.
;**********************************************************
;        .LOC    START+0
         ;ORG START+0
;M1:     .WORD   TBASE
M1       DD TBASE
;M2:     .WORD   TBASE
M2       DD TBASE
;M3:     .WORD   TBASE
M3       DD TBASE
;M4:     .WORD   TBASE
M4       DD TBASE
;T1:     .WORD   TBASE
T1       DD TBASE
;T2:     .WORD   TBASE
T2       DD TBASE
;T3:     .WORD   TBASE
T3       DD TBASE
;INDX1:  .WORD   TBASE
INDX1    DD TBASE
;INDX2:  .WORD   TBASE
INDX2    DD TBASE
;NPINS:  .WORD   TBASE
NPINS    DD TBASE
;MLPTRI: .WORD   PLYIX
MLPTRI   DD PLYIX
;MLPTRJ: .WORD   0
MLPTRJ   DD 0
;SCRIX:  .WORD   0
SCRIX    DD 0
;BESTM:  .WORD   0
BESTM    DD 0
;MLLST:  .WORD   0
MLLST    DD 0
;MLNXT:  .WORD   MLIST
MLNXT    DD MLIST

;X p25
;**********************************************************
; VARIABLES SECTION
;
; KOLOR --      Indicates computer's color. White is 0, and
;               Black is 80H.
;
; COLOR --      Indicates color of the side with the move.
;
; P1-P3 --      Working area to hold the contents of the board
;               array for a given square.
;
; PMATE --      The move number at which a checkmate is
;               discovered during look ahead.
;
; MOVENO --     Current move number.
;
; PLYMAX --     Maximum depth of search using Alpha-Beta
;               pruning.
;
; NPLY  --      Current ply number during Alpha-Beta
;               pruning.
;
; CKFLG --      A non-zero value indicates the king is in check.
;
; MATEF --      A zero value indicates no legal moves.
;
; VALM  --      The score of the current move being examined.
;
; BRDC  --      A measure of mobility equal to the total number
;               of squares white can move to minus the number
;               black can move to.
;
; PTSL  --      The maximum number of points which could be lost
;               through an exchange by the player not on the
;               move.
;
; PTSW1 --      The maximum number of points which could be won
;               through an exchange by the player not on the
;               move.
;
; PTSW2 --      The second highest number of points which could
;               be won through a different exchange by the player
;               not on the move.
;
; MTRL  --      A measure of the difference in material
;               currently on the board. It is the total value of
;               the white pieces minus the total value of the
;               black pieces.
;
; BC0   --      The value of board control(BRDC) at ply 0.
;X p26
;
;
; MV0   --      The value of material(MTRL) at ply 0.
;
; PTSCK --      A non-zero value indicates that the piece has
;               just moved itself into a losing exchange of
;               material.
;
; BMOVES --     Our very tiny book of openings. Determines
;               the first move for the computer.
;
;**********************************************************
;KOLOR:  .BYTE   0
KOLOR    DB 0
;COLOR:  .BYTE   0
COLOR    DB 0
;P1:     .BYTE   0
P1       DB 0
;P2:     .BYTE   0
P2       DB 0
;P3:     .BYTE   0
P3       DB 0
;PMATE:  .BYTE   0
PMATE    DB 0
;MOVENO: .BYTE   0
MOVENO   DB 0
;PLYMAX: .BYTE   2
PLYMAX   DB 2
;NPLY:   .BYTE   0
NPLY     DB 0
;CKFLG:  .BYTE   0
CKFLG    DB 0
;MATEF:  .BYTE   0
MATEF    DB 0
;VALM:   .BYTE   0
VALM     DB 0
;BRDC:   .BYTE   0
BRDC     DB 0
;PTSL:   .BYTE   0
PTSL     DB 0
;PTSW1:  .BYTE   0
PTSW1    DB 0
;PTSW2:  .BYTE   0
PTSW2    DB 0
;MTRL:   .BYTE   0
MTRL     DB 0
;BC0:    .BYTE   0
BC0      DB 0
;MV0:    .BYTE   0
MV0      DB 0
;PTSCK:  .BYTE   0
PTSCK    DB 0
;BMOVES: .BYTE   35,55,10H
BMOVES   DB 35, 55, 10H
;        .BYTE   34,54,10H
         DB 34, 54, 10H
;        .BYTE   85,65,10H
         DB 85, 65, 10H
;        .BYTE   84,64,10H
         DB 84, 64, 10H

;X p27
;**********************************************************
; MOVE LIST SECTION
;
; MLIST --      A 2048 byte storage area for generated moves.
;               This area must be large enough to hold all
;               the moves for a single leg of the move tree.
;
; MLEND --      The address of the last available location
;               in the move list.
;
; MLPTR --      The Move List is a linked list of individual
;               moves each of which is 6 bytes in length. The
;               move list pointer(MLPTR) is the link field
;               within a move.
;
; MLFRP --      The field in the move entry which gives the
;               board position from which the piece is moving.
;
; MLTOP --      The field in the move entry which gives the
;               board position to which the piece is moving.
;
; MLFLG --      A field in the move entry which contains flag
;               information. The meaning of each bit is as
;               follows:
;               Bit 7 -- The color of any captured piece
;                       0 -- White
;                       1 -- Black
;               Bit 6 -- Double move flag (set for castling and
;                       en passant pawn captures)
;               Bit 5 -- Pawn Promotion flag; set when pawn
;                       promotes.
;               Bit 4 -- When set, this flag indicates that
;                       this is the first move for the
;                       piece on the move.
;               Bit 3 -- This flag is set is there is a piece
;                       captured, and that piece has moved at
;                       least once.
;               Bits 2-0 Describe the captured piece. A
;                       zero value indicates no capture.
;
; MLVAL --      The field in the move entry which contains the
;               score assigned to the move.
;
;**********************************************************


;*** TEMP TODO BEGIN
;MVEMSG:   .WORD 0
MVEMSG   DD 0
;MVEMSG_2: .WORD 0
MVEMSG_2 DD 0
;BRDPOS: .BLKB   1       ; Index into the board array
BRDPOS   DB 1 DUP (?)
;ANBDPS: .BLKB   1       ; Additional index required for ANALYS
ANBDPS   DB 1 DUP (?)
;LINECT: .BYTE   0       ; Current line number
LINECT   DB 0
;**** TEMP TODO END

;X p28
;        .LOC    START+3F0H      ;X START+300H
         ;ORG START+3F0H
;MLIST:  .BLKB   2048
MLIST    DB 2048 DUP (?)
;MLEND:  .WORD   0
MLEND    DD 0
;MLPTR   =       0
MLPTR    EQU     0
;MLFRP   =       4
MLFRP    EQU     4
;MLTOP   =       5
MLTOP    EQU     5
;MLFLG   =       6
MLFLG    EQU     6
;MLVAL   =       7
MLVAL    EQU     7
;        .BLKB   100    ;todo headroom
         DB 100 DUP (?)

;**********************************************************
; PROGRAM CODE SECTION
;**********************************************************
;        .CODE
_DATA    ENDS
_TEXT    SEGMENT
;Z80_EXAF:   RET
Z80_EXAF:        RET
;Z80_EXX:    RET
Z80_EXX: RET
;Z80_RLD:    RET
Z80_RLD: RET
;Z80_RRD:    RET
Z80_RRD: RET
;Z80_LDAR:   RET
Z80_LDAR:        RET
;FCDMAT:     RET
FCDMAT:  RET
;TBCPMV:     RET
TBCPMV:  RET
;INSPCE:     RET
INSPCE:  RET
;BLNKER:     RET
BLNKER:  RET
        
;**********************************************************
; BOARD SETUP ROUTINE
;**********************************************************
; FUNCTION:     To initialize the board array, setting the
;               pieces in their initial positions for the
;               start of the game.
;
; CALLED BY:    DRIVER
;
; CALLS:        None
;
; ARGUMENTS:    None
;**********************************************************
INITBD  PROC
        cmp al,0
        jnz CPTRMV
;INITBD: MVI     B,120           ; Pre-fill board with -1's
         MOV ch,120
;        LXI     H,BOARDA
         MOV ebx,offset BOARDA
;back01: MVI     M,-1
back01:  MOV byte ptr [ebx],-1
;        INX     H
         LAHF
         INC ebx
         SAHF
;        DJNZ    back01
         LAHF
         DEC ch
         JNZ back01
         SAHF
;        MVI     B,8
         MOV ch,8
;        LXI     X,BOARDA
         MOV esi,offset BOARDA
;IB2:    MOV     A,-8(X)         ; Fill non-border squares
IB2:     MOV al,byte ptr [esi-8]
;        MOV     21(X),A         ; White pieces
         MOV byte ptr [esi+21],al
;        SET     7,A             ; Change to black
         LAHF
         OR al,80h
         SAHF
;        MOV     91(X),A         ; Black pieces
         MOV byte ptr [esi+91],al
;        MVI     31(X),PAWN      ; White Pawns
         MOV byte ptr [esi+31],PAWN
;        MVI     81(X),BPAWN     ; Black Pawns
         MOV byte ptr [esi+81],BPAWN
;        MVI     41(X),0         ; Empty squares
         MOV byte ptr [esi+41],0
;        MVI     51(X),0
         MOV byte ptr [esi+51],0
;        MVI     61(X),0
         MOV byte ptr [esi+61],0
;        MVI     71(X),0
         MOV byte ptr [esi+71],0
;        INX     X
         LAHF
         INC esi
         SAHF
;        DJNZ    IB2
         LAHF
         DEC ch
         JNZ IB2
         SAHF
;        LXI     X,POSK          ; Init King/Queen position list
         MOV esi,offset POSK
;        MVI     0(X),25
         MOV byte ptr [esi+0],25
;        MVI     1(X),95
         MOV byte ptr [esi+1],95
;        MVI     2(X),24
         MOV byte ptr [esi+2],24
;        MVI     3(X),94
         MOV byte ptr [esi+3],94
;        RET
         RET
;X p29
;**********************************************************
; PATH ROUTINE
;**********************************************************
; FUNCTION:     To generate a single possible move for a given
;               piece along its current path of motion including:
;               Fetching the contents of the board at the new
;               position, and setting a flag describing the
;               contents:
;                       0 --    New position is empty
;                       1 --    Encountered a piece of the
;                               opposite color
;                       2 --    Encountered a piece of the
;                               same color
;                       3 --    New position is off the
;                               board
;
; CALLED BY:    MPIECE
;               ATTACK
;               PINFND
;
; CALLS:        None
;
; ARGUMENTS:    Direction from the direction array giving the
;               constant to be added for the new position.
;
;**********************************************************
;PATH:   LXI     H,M2    ; Get previous position
PATH:    MOV ebx,offset M2
;        MOV     A,M
         MOV al,byte ptr [ebx]
;        ADD     C       ; Add diection constant
         ADD al,cl
;        MOV     M,A     ; Save new position
         MOV byte ptr [ebx],al
;        LIXD    M2      ; Load board index
         MOV esi,[M2]
;        MOV     A,BOARD(X)      ; Get contents of board
         MOV al,byte ptr [esi+BOARD]
;        CPI     -1      ; In boarder area ?
         CMP al,-1
;        JRZ     PA2     ; Yes - jump
         JZ PA2
;        STA     P2      ; Save piece
         MOV byte ptr [P2],al
;        ANI     7       ; Clear flags
         AND al,7
;        STA     T2      ; Save piece type
         MOV byte ptr [T2],al
;        RZ              ; Return if empty
         JNZ skip1
         RET
skip1:
;        LDA     P2      ; Get piece encountered
         MOV AL,byte ptr [P2]
;        LXI     H,P1    ; Get moving piece address
         MOV ebx,offset P1
;        XRA     M       ; Compare
         XOR al,byte ptr [ebx]
;        BIT     7,A     ; Do colors match ?
         MOV ah,al
         AND ah,80h
;        JRZ     PA1     ; Yes - jump
         JZ PA1
;        MVI     A,1     ; Set different color flag
         MOV al,1
;        RET             ; Return
         RET
;PA1:    MVI     A,2     ; Set same color flag
PA1:     MOV al,2
;        RET             ; Return
         RET
;PA2:    MVI     A,3     ; Set off board flag
PA2:     MOV al,3
;        RET             ; Return
         RET

;X p30
;*****************************************************************
; PIECE MOVER ROUTINE
;*****************************************************************
;
; FUNCTION:     To generate all the possible legal moves for a
;               given piece.
;
; CALLED BY:    GENMOV
;
; CALLS:        PATH
;               ADMOVE
;               CASTLE
;               ENPSNT
;
; ARGUMENTS:    The piece to be moved.
;*****************************************************************
;MPIECE: XRA     M       ; Piece to move
MPIECE:  XOR al,byte ptr [ebx]
;        ANI     87H     ; Clear flag bit
         AND al,87H
;        CPI     BPAWN   ; Is it a black Pawn ?
         CMP al,BPAWN
;        JRNZ    rel001  ; No-Skip
         JNZ rel001
;        DCR     A       ; Decrement for black Pawns
         DEC al
;rel001: ANI     7       ; Get piece type
rel001:  AND al,7
;        STA     T1      ; Save piece type
         MOV byte ptr [T1],al
;        LIYD    T1      ; Load index to DCOUNT/DPOINT
         MOV edi,[T1]
;        MOV     B,DCOUNT(Y) ; Get direction count
         MOV ch,byte ptr [edi+DCOUNT]
;        MOV     A,DPOINT(Y) ; Get direction pointer
         MOV al,byte ptr [edi+DPOINT]
;        STA     INDX2   ; Save as index to direct
         MOV byte ptr [INDX2],al
;        LIYD    INDX2   ; Load index
         MOV edi,[INDX2]
;MP5:    MOV     C,DIRECT(Y)     ; Get move direction
MP5:     MOV cl,byte ptr [edi+DIRECT]
;        LDA     M1      ; From position
         MOV AL,byte ptr [M1]
;        STA     M2      ; Initialize to position
         MOV byte ptr [M2],al
;MP10:   CALL    PATH    ; Calculate next position
MP10:    CALL PATH
;        CPI     2       ; Ready for new direction ?
         CMP al,2
;        JRNC    MP15    ; Yes - Jump
         JNC MP15
;        ANA     A       ; Test for empty square
         AND al,al
;        EXAF            ; Save result
         CALL Z80_EXAF
;        LDA     T1      ; Get piece moved
         MOV AL,byte ptr [T1]
;        CPI     PAWN+1  ; Is it a Pawn ?
         CMP al,PAWN+1
;        JRC     MP20    ; Yes - Jump
         JC MP20
;        CALL    ADMOVE  ; Add move to list
         CALL ADMOVE
;        EXAF            ; Empty square ?
         CALL Z80_EXAF
;        JRNZ    MP15    ; No - Jump
         JNZ MP15
;        LDA     T1      ; Piece type
         MOV AL,byte ptr [T1]
;        CPI     KING    ; King ?
         CMP al,KING
;        JRZ     MP15    ; Yes - Jump
         JZ MP15
;        CPI     BISHOP  ; Bishop, Rook, or Queen ?
         CMP al,BISHOP
;        JRNC    MP10    ; Yes - Jump
         JNC MP10
;MP15:   INX     Y       ; Increment direction index
MP15:    LAHF
         INC edi
         SAHF
;        DJNZ    MP5     ; Decr. count-jump if non-zerc
         LAHF
         DEC ch
         JNZ MP5
         SAHF
;        LDA     T1      ; Piece type
         MOV AL,byte ptr [T1]
;X p31
;        CPI     KING    ; King ?
         CMP al,KING
;        CZ      CASTLE  ; Yes - Try Castling
         JNZ skip2
         CALL CASTLE
skip2:
;        RET             ; Return
         RET
; ***** PAWN LOGIC *****
;MP20:   MOV     A,B     ; Counter for direction
MP20:    MOV al,ch
;        CPI     3       ; On diagonal moves ?
         CMP al,3
;        JRC     MP35    ; Yes - Jump
         JC MP35
;        JRZ     MP30    ; -or-jump if on 2 square move
         JZ MP30
;        EXAF            ; Is forward square empty?
         CALL Z80_EXAF
;        JRNZ    MP15    ; No - jump
         JNZ MP15
;        LDA     M2      ; Get "to" position
         MOV AL,byte ptr [M2]
;        CPI     91      ; Promote white Pawn ?
         CMP al,91
;        JRNC    MP25    ; Yes - Jump
         JNC MP25
;        CPI     29      ; Promote black Pawn ?
         CMP al,29
;        JRNC    MP26    ; No - Jump
         JNC MP26
;MP25:   LXI     H,P2    ; Flag address
MP25:    MOV ebx,offset P2
;        SET     5,M     ; Set promote flag
         LAHF
         OR byte ptr [ebx],20h
         SAHF
;MP26:   CALL    ADMOVE  ; Add to move list
MP26:    CALL ADMOVE
;        INX     Y       ; Adjust to two square move
         LAHF
         INC edi
         SAHF
;        DCR     B
         DEC ch
;        LXI     H,P1    ; Check Pawn moved flag
         MOV ebx,offset P1
;        BIT     3,M     ; Has it moved before ?
         MOV ah,byte ptr [ebx]
         AND ah,8
;        JRZ     MP10    ; No - Jump
         JZ MP10
;        JMP     MP15    ; Jump
         JMP MP15
;MP30:   EXAF            ; Is forward square empty ?
MP30:    CALL Z80_EXAF
;        JRNZ    MP15    ; No - Jump
         JNZ MP15
;MP31:   CALL    ADMOVE  ; Add to move list
MP31:    CALL ADMOVE
;        JMP     MP15    ; Jump
         JMP MP15
;MP35:   EXAF            ; Is diagonal square empty ?
MP35:    CALL Z80_EXAF
;        JRZ     MP36    ; Yes - Jump
         JZ MP36
;        LDA     M2      ; Get "to" position
         MOV AL,byte ptr [M2]
;        CPI     91      ; Promote white Pawn ?
         CMP al,91
;        JRNC    MP37    ; Yes - Jump
         JNC MP37
;        CPI     29      ; Black Pawn promotion ?
         CMP al,29
;        JRNC    MP31    ; No- Jump
         JNC MP31
;MP37:   LXI     H,P2    ; Get flag address
MP37:    MOV ebx,offset P2
;        SET     5,M     ; Set promote flag
         LAHF
         OR byte ptr [ebx],20h
         SAHF
;        JMPR    MP31    ; Jump
         JMP MP31
;MP36:   CALL    ENPSNT  ; Try en passant capture
MP36:    CALL ENPSNT
;        JMP     MP15    ; Jump
         JMP MP15

;X p32
;**********************************************************
; EN PASSANT ROUTINE
;**********************************************************
; FUNCTION:     --      To test for en passant Pawn capture and
;                       to add it to the move list if it is
;                       legal.
;
; CALLED BY:    --      MPIECE
;
; CALLS:        --      ADMOVE
;                       ADJPTR
;
; ARGUMENTS:    --      None
;**********************************************************
;ENPSNT: LDA     M1      ; Set position of Pawn
ENPSNT:  MOV AL,byte ptr [M1]
;        LXI     H,P1    ; Check color
         MOV ebx,offset P1
;        BIT     7,M     ; Is it white ?
         MOV ah,byte ptr [ebx]
         AND ah,80h
;        JRZ     rel002  ; Yes - skip
         JZ rel002
;        ADI     10      ; Add 10 for black
         ADD al,10
;rel002: CPI     61      ; On en passant capture rank ?
rel002:  CMP al,61
;        RC              ; No - return
         JNC skip3
         RET
skip3:
;        CPI     69      ; On en passant capture rank ?
         CMP al,69
;        RNC             ; No - return
         JC skip4
         RET
skip4:
;        LIXD    MLPTRJ  ; Get pointer to previous move
         MOV esi,[MLPTRJ]
;        BIT     4,MLFLG(X) ; First move for that piece ?
         MOV ah,byte ptr [esi+MLFLG]
         AND ah,10h
;        RZ              ; No - return
         JNZ skip5
         RET
skip5:
;        MOV     A,MLTOP(X) ; Get "to" postition
         MOV al,byte ptr [esi+MLTOP]
;        STA     M4      ; Store as index to board
         MOV byte ptr [M4],al
;        LIXD    M4      ; Load board index
         MOV esi,[M4]
;        MOV     A,BOARD(X) ; Get piece moved
         MOV al,byte ptr [esi+BOARD]
;        STA     P3      ; Save it
         MOV byte ptr [P3],al
;        ANI     7       ; Get piece type
         AND al,7
;        CPI     PAWN    ; Is it a Pawn ?
         CMP al,PAWN
;        RNZ             ; No - return
         JZ skip6
         RET
skip6:
;        LDA     M4      ; Get "to" position
         MOV AL,byte ptr [M4]
;        LXI     H,M2    ; Get present "to" position
         MOV ebx,offset M2
;        SUB     M       ; Find difference
         SUB al,byte ptr [ebx]
;        JP      rel003  ; Positive ? Yes - Jump
         JP rel003
;        NEG             ; Else take absolute value
         NEG al
;        CPI     10      ; Is difference 10 ?
         CMP al,10
;rel003: RNZ             ; No - return
rel003:  JZ skip7
         RET
skip7:
;        LXI     H,P2    ; Address of flags
         MOV ebx,offset P2
;        SET     6,M     ; Set double move flag
         LAHF
         OR byte ptr [ebx],40h
         SAHF
;        CALL    ADMOVE  ; Add Pawn move to move list
         CALL ADMOVE
;        LDA     M1      ; Save initial Pawn position
         MOV AL,byte ptr [M1]
;        STA     M3
         MOV byte ptr [M3],al
;        LDA     M4      ; Set "from" and "to" positions
         MOV AL,byte ptr [M4]
                        ; for dummy move
;X p33
;        STA     M1
         MOV byte ptr [M1],al
;        STA     M2
         MOV byte ptr [M2],al
;        LDA     P3      ; Save captured Pawn
         MOV AL,byte ptr [P3]
;        STA     P2
         MOV byte ptr [P2],al
;        CALL    ADMOVE  ; Add Pawn capture to move list
         CALL ADMOVE
;        LDA     M3      ; Restore "from" position
         MOV AL,byte ptr [M3]
;        STA     M1
         MOV byte ptr [M1],al

;*****************************************************************
; ADJUST MOVE LIST POINTER FOR DOUBLE MOVE
;*****************************************************************
; FUNCTION: --  To adjust move list pointer to link around
;               second move in double move.
;
; CALLED BY: -- ENPSNT
;               CASTLE
;               (This mini-routine is not really called,
;               but is jumped to to save time.)
;
; CALLS:    --  None
;
; ARGUMENTS: -- None
;*****************************************************************
;ADJPTR: LHLD    MLLST   ; Get list pointer
ADJPTR:  MOV ebx,[MLLST]
;        LXI     D,-8    ; Size of a move entry
         MOV edx,-8
;        DAD     D       ; Back up list pointer
         LAHF
         ADD ebx,edx
         SAHF
;        SHLD    MLLST   ; Save list pointer
         MOV [MLLST],ebx
;        MVI     M,0     ; Zero out link, first byte
         MOV byte ptr [ebx],0
;        INX     H       ; Next byte
         LAHF
         INC ebx
         SAHF
;        MVI     M,0     ; Zero out link, second byte
         MOV byte ptr [ebx],0
;        INX     H       ; Next byte
         LAHF
         INC ebx
         SAHF
;        MVI     M,0     ; Zero out link, third byte
         MOV byte ptr [ebx],0
;        INX     H       ; Next byte
         LAHF
         INC ebx
         SAHF
;        MVI     M,0     ; Zero out link, fourth byte
         MOV byte ptr [ebx],0
;        RET             ; Return
         RET

;X p34
;*****************************************************************
; CASTLE ROUTINE
;*****************************************************************
; FUNCTION: --  To determine whether castling is legal
;               (Queen side, King side, or both) and add it
;               to the move list if it is.
;
; CALLED BY: -- MPIECE
;
; CALLS:   --   ATTACK
;               ADMOVE
;               ADJPTR
;
; ARGUMENTS: -- None
;*****************************************************************
;CASTLE: LDA     P1      ; Get King
CASTLE:  MOV AL,byte ptr [P1]
;        BIT     3,A     ; Has it moved ?
         MOV ah,al
         AND ah,8
;        RNZ             ; Yes - return
         JZ skip8
         RET
skip8:
;        LDA     CKFLG   ; Fetch Check Flag
         MOV AL,byte ptr [CKFLG]
;        ANA     A       ; Is the King in check ?
         AND al,al
;        RNZ             ; Yes - Return
         JZ skip9
         RET
skip9:
;        LXI     B,0FF03H ; Initialize King-side values
         MOV ecx,0FF03H
;CA5:    LDA     M1      ; King position
CA5:     MOV AL,byte ptr [M1]
;        ADD     C       ; Rook position
         ADD al,cl
;        MOV     C,A     ; Save
         MOV cl,al
;        STA     M3      ; Store as board index
         MOV byte ptr [M3],al
;        LIXD    M3      ; Load board index
         MOV esi,[M3]
;        MOV     A,BOARD(X) ; Get contents of board
         MOV al,byte ptr [esi+BOARD]
;        ANI     7FH     ; Clear color bit
         AND al,7FH
;        CPI     ROOK    ; Has Rook ever moved ?
         CMP al,ROOK
;        JRNZ    CA20    ; Yes - Jump
         JNZ CA20
;        MOV     A,C     ; Restore Rook position
         MOV al,cl
;        JMPR    CA15    ; Jump
         JMP CA15
;CA10:   LIXD    M3      ; Load board index
CA10:    MOV esi,[M3]
;        MOV     A,BOARD(X) ; Get contents of board
         MOV al,byte ptr [esi+BOARD]
;        ANA     A       ; Empty ?
         AND al,al
;        JRNZ    CA20    ; No - Jump
         JNZ CA20
;        LDA     M3      ; Current position
         MOV AL,byte ptr [M3]
;        CPI     22      ; White Queen Knight square ?
         CMP al,22
;        JRZ     CA15    ; Yes - Jump
         JZ CA15
;        CPI     92      ; Black Queen Knight square ?
         CMP al,92
;        JRZ     CA15    ; Yes - Jump
         JZ CA15
;        CALL    ATTACK  ; Look for attack on square
         CALL ATTACK
;        ANA     A       ; Any attackers ?
         AND al,al
;        JRNZ    CA20    ; Yes - Jump
         JNZ CA20
;        LDA     M3      ; Current position
         MOV AL,byte ptr [M3]
;CA15:   ADD     B       ; Next position
CA15:    ADD al,ch
;        STA     M3      ; Save as board index
         MOV byte ptr [M3],al
;        LXI     H,M1    ; King position
         MOV ebx,offset M1
;        CMP     M       ; Reached King ?
         CMP al,byte ptr [ebx]
;X p35
;        JRNZ    CA10    ; No - jump
         JNZ CA10
;        SUB     B       ; Determine King's position
         SUB al,ch
;        SUB     B
         SUB al,ch
;        STA     M2      ; Save it
         MOV byte ptr [M2],al
;        LXI     H,P2    ; Address of flags
         MOV ebx,offset P2
;        MVI     M,40H   ; Set double move flag
         MOV byte ptr [ebx],40H
;        CALL    ADMOVE  ; Put king move in list
         CALL ADMOVE
;        LXI     H,M1    ; Addr of King "from" position
         MOV ebx,offset M1
;        MOV     A,M     ; Get King's "from" position
         MOV al,byte ptr [ebx]
;        MOV     M,C     ; Store Rook "from" position
         MOV byte ptr [ebx],cl
;        SUB     B       ; Get Rook "to" position
         SUB al,ch
;        STA     M2      ; Store Rook "to" position
         MOV byte ptr [M2],al
;        XRA     A       ; Zero
         XOR al,al
;        STA     P2      ; Zero move flags
         MOV byte ptr [P2],al
;        CALL    ADMOVE  ; Put Rook move in list
         CALL ADMOVE
;        CALL    ADJPTR  ; Re-adjust move list pointer
         CALL ADJPTR
;        LDA     M3      ; Restore King position
         MOV AL,byte ptr [M3]
;        STA     M1      ; Store
         MOV byte ptr [M1],al
;CA20:   MOV     A,B     ; Scan Index
CA20:    MOV al,ch
;        CPI     1       ; Done ?
         CMP al,1
;        RZ              ; Yes - return
         JNZ skip10
         RET
skip10:
;        LXI     B,01FCH ; Set Queen-side initial values
         MOV ecx,01FCH
;        JMP     CA5     ; Jump
         JMP CA5

;X p36
;**********************************************************
; ADMOVE ROUTINE
;**********************************************************
; FUNCTION: --  To add a move to the move list
;
; CALLED BY: -- MPIECE
;               ENPSNT
;               CASTLE
;
; CALLS: --     None
;
; ARGUMENT: --  None
;**********************************************************
;ADMOVE: LDED    MLNXT   ; Addr of next loc in move list
ADMOVE:  MOV edx,[MLNXT]
;        LXI     H,MLEND ; Address of list end
         MOV ebx,offset MLEND
;        ANA     A       ; Clear carry flag
         AND al,al
;        DSBC    D       ; Calculate difference
         SBB ebx,edx
;        JRC     AM10    ; Jump if out of space
         JC AM10
;        LHLD    MLLST   ; Addr of prev. list area
         MOV ebx,[MLLST]
;        SDED    MLLST   ; Savn next as previous
         MOV [MLLST],edx
;        MOV     M,E     ; Store link address
         MOV dword ptr [ebx],edx
;        LXI     H,P1    ; Address of moved piece
         MOV ebx,offset P1
;        BIT     3,M     ; Has it moved before ?
         MOV ah,byte ptr [ebx]
         AND ah,8
;        JRNZ    rel004  ; Yes - jump
         JNZ rel004
;        LXI     H,P2    ; Address of move flags
         MOV ebx,offset P2
;        SET     4,M     ; Set first move flag
         LAHF
         OR byte ptr [ebx],10h
         SAHF
;rel004: XCHG            ; Address of move area
rel004:  XCHG ebx,edx
;        MVI     M,0     ; Store zero in link address
         mov dword ptr [ebx],0
         add ebx,4
;        LDA     M1      ; Store "from" move position
         MOV AL,byte ptr [M1]
;        MOV     M,A
         MOV byte ptr [ebx],al
;        INX     H
         LAHF
         INC ebx
         SAHF
;        LDA     M2      ; Store "to" move position
         MOV AL,byte ptr [M2]
;        MOV     M,A
         MOV byte ptr [ebx],al
;        INX     H
         LAHF
         INC ebx
         SAHF
;        LDA     P2      ; Store move flags/capt. piece
         MOV AL,byte ptr [P2]
;        MOV     M,A
         MOV byte ptr [ebx],al
;        INX     H
         LAHF
         INC ebx
         SAHF
;        MVI     M,0     ; Store initial move value
         MOV byte ptr [ebx],0
;        INX     H
         LAHF
         INC ebx
         SAHF
;        SHLD    MLNXT   ; Save address for next move
         MOV [MLNXT],ebx
;        RET             ; Return
         RET
;AM10:   MVI     M,0     ; Abort entry on table ovflow
AM10:    MOV byte ptr [ebx],0
;        INX     H
         LAHF
         INC ebx
         SAHF
;        MVI     M,0
         MOV byte ptr [ebx],0
;        DCX     H
         LAHF
         DEC ebx
         SAHF
;        RET
         RET

;X p37
;**********************************************************
; GENERATE MOVE ROUTINE
;**********************************************************
; FUNCTION: --  To generate the move set for all of the
;               pieces of a given color.
;
; CALLED BY: -- FNDMOV
;
; CALLS: --     MPIECE
;               INCHK
;
; ARGUMENTS: -- None
;**********************************************************
;GENMOV: CALL    INCHK   ; Test for King in check
GENMOV:  CALL INCHK
;        STA     CKFLG   ; Save attack count as flag
         MOV byte ptr [CKFLG],al
;        LDED    MLNXT   ; Addr of next avail list space
         MOV edx,[MLNXT]
;        LHLD    MLPTRI  ; Ply list pointer index
         MOV ebx,[MLPTRI]
;        INX     H       ; Increment to next ply
         LAHF
         INC ebx
         SAHF
;        INX     H
         LAHF
         INC ebx
         SAHF
;        MOV     M,E     ; Save move list pointer
         MOV byte ptr [ebx],dl
;        INX     H
         LAHF
         INC ebx
         SAHF
;        MOV     M,D
         MOV byte ptr [ebx],dh
;        INX     H
         LAHF
         INC ebx
         SAHF
;        SHLD    MLPTRI  ; Save new index
         MOV [MLPTRI],ebx
;        SHLD    MLLST   ; Last pointer for chain init.
         MOV [MLLST],ebx
;        MVI     A,21    ; First position on board
         MOV al,21
;GM5:    STA     M1      ; Save as index
GM5:     MOV byte ptr [M1],al
;        LIXD    M1      ; Load board index
         MOV esi,[M1]
;        MOV     A,BOARD(X) ; Fetch board contents
         MOV al,byte ptr [esi+BOARD]
;        ANA     A       ; Is it empty ?
         AND al,al
;        JRZ     GM10    ; Yes - Jump
         JZ GM10
;        CPI     -1      ; Is it a boarder square ?
         CMP al,-1
;        JRZ     GM10    ; Yes - Jump
         JZ GM10
;        STA     P1      ; Save piece
         MOV byte ptr [P1],al
;        LXI     H,COLOR ; Address of color of piece
         MOV ebx,offset COLOR
;        XRA     M       ; Test color of piece
         XOR al,byte ptr [ebx]
;        BIT     7,A     ; Match ?
         MOV ah,al
         AND ah,80h
;        CZ      MPIECE  ; Yes - call Move Piece
         JNZ skip11
         CALL MPIECE
skip11:
;GM10:   LDA     M1      ; Fetch current board position
GM10:    MOV AL,byte ptr [M1]
;        INR     A       ; Incr to next board position
         INC al
;        CPI     99      ; End of board array ?
         CMP al,99
;        JNZ     GM5     ; No - Jump
         JNZ GM5
;        RET             ; Return
         RET

;X p38
;**********************************************************
; CHECK ROUTINE
;**********************************************************
; FUNCTION: --  To determine whether or not the
;               King is in check.
;
; CALLED BY: -- GENMOV
;               FNDMOV
;               EVAL
;
; CALLS: --     ATTACK
;
; ARGUMENTS: -- Color of King
;**********************************************************
;INCHK:  LDA     COLOR   ; Get color
INCHK:   MOV AL,byte ptr [COLOR]
;INCHK1: LXI     H,POSK  ; Addr of white King position
INCHK1:  MOV ebx,offset POSK
;        ANA     A       ; White ?
         AND al,al
;        JRZ     rel005  ; Yes - Skip
         JZ rel005
;        INX     H       ; Addr of black King position
         LAHF
         INC ebx
         SAHF
;rel005: MOV     A,M     ; Fetch King position
rel005:  MOV al,byte ptr [ebx]
;        STA     M3      ; Save
         MOV byte ptr [M3],al
;        LIXD    M3      ; Load board index
         MOV esi,[M3]
;        MOV     A,BOARD(X) ; Fetch board contents
         MOV al,byte ptr [esi+BOARD]
;        STA     P1      ; Save
         MOV byte ptr [P1],al
;        ANI     7       ; Get piece type
         AND al,7
;        STA     T1      ; Save
         MOV byte ptr [T1],al
;        CALL    ATTACK  ; Look for attackers on King
         CALL ATTACK
;        RET             ; Return
         RET

;**********************************************************
; ATTACK ROUTINE
;**********************************************************

; FUNCTION: --  To find all attackers on a given square
;               by scanning outward from the square
;               until a piece is found that attacks
;               that square, or a piece is found that
;               doesn't attack that square, or the edge
;               of the board is reached.
;
;                       In determining which pieces attack a square,
;               this routine also takes into account the ability of
;               certain pieces to attack through another attacking
;               piece. (For example a queen lined up behind a bishop
;               of her same color along a diagonal.) The bishop is
;               then said to be transparent to the queen, since both
;               participate in the attack.
;
;                       In the case where this routine is called by
;               CASTLE or INCHK, the routine is terminated as soon as
;               an attacker of the opposite color is encountered.
;
; CALLED BY: -- POINTS
;               PINFND
;               CASTLE
;               INCHK
;
; CALLS: --     PATH
;               ATKSAV
;
; ARGUMENTS: -- None
;*****************************************************************
;ATTACK: PUSH    B       ; Save Register B
ATTACK:  PUSH ecx
;        XRA     A       ; Clear
         XOR al,al
;        MVI     B,16    ; Initial direction count
         MOV ch,16
;        STA     INDX2   ; Initial direction index
         MOV byte ptr [INDX2],al
;        LIYD    INDX2   ; Load index
         MOV edi,[INDX2]
;AT5:    MOV     C,DIRECT(Y) ; Get direction
AT5:     MOV cl,byte ptr [edi+DIRECT]
;        MVI     D,0     ; Init. scan count/flags
         MOV dh,0
;        LDA     M3      ; Init. board start position
         MOV AL,byte ptr [M3]
;        STA     M2      ; Save
         MOV byte ptr [M2],al
;AT10:   INR     D       ; Increment scan count
AT10:    INC dh
;        CALL    PATH    ; Next position
         CALL PATH
;        CPI     1       ; Piece of a opposite color ?
         CMP al,1
;        JRZ     AT14A   ; Yes - jump
         JZ AT14A
;        CPI     2       ; Piece of same color ?
         CMP al,2
;        JRZ     AT14B   ; Yes - jump
         JZ AT14B
;        ANA     A       ; Empty position ?
         AND al,al
;        JRNZ    AT12    ; No - jump
         JNZ AT12
;        MOV     A,B     ; Fetch direction count
         MOV al,ch
;        CPI     9       ; On knight scan ?
         CMP al,9
;        JRNC    AT10    ; No - jump
         JNC AT10
;AT12:   INX     Y       ; Increment direction index
AT12:    LAHF
         INC edi
         SAHF
;        DJNZ    AT5     ; Done ? No - jump
         LAHF
         DEC ch
         JNZ AT5
         SAHF
;        XRA     A       ; No attackers
         XOR al,al
;AT13:   POP     B       ; Restore register B
AT13:    POP ecx
;        RET             ; Return
         RET
;AT14A:  BIT     6,D     ; Same color found already ?
AT14A:   MOV ah,dh
         AND ah,40h
;        JRNZ    AT12    ; Yes - jump
         JNZ AT12
;        SET     5,D     ; Set opposite color found flag
         LAHF
         OR dh,20h
         SAHF
;        JMP     AT14    ; Jump
         JMP AT14
;AT14B:  BIT     5,D     ; Opposite color found already?
AT14B:   MOV ah,dh
         AND ah,20h
;        JRNZ    AT12    ; Yes - jump
         JNZ AT12
;        SET     6,D     ; Set same color found flag
         LAHF
         OR dh,40h
         SAHF

; ***** DETERMINE IF PIECE ENCOUNTERED ATTACKS SQUARE *****
;AT14:   LDA     T2      ; Fetch piece type encountered
AT14:    MOV AL,byte ptr [T2]
;        MOV     E,A     ; Save
         MOV dl,al
;        MOV     A,B     ; Get direction-counter
         MOV al,ch
;        CPI     9       ; Look for Knights ?
         CMP al,9
;        JRC     AT25    ; Yes - jump
         JC AT25
;        MOV     A,E     ; Get piece type
         MOV al,dl
;        CPI     QUEEN   ; Is is a Queen ?
         CMP al,QUEEN
;        JRNZ    AT15    ; No - Jump
         JNZ AT15
;        SET     7,D     ; Set Queen found flag
         LAHF
         OR dh,80h
         SAHF
;        JMPR    AT30    ; Jump
         JMP AT30
;AT15:   MOV     A,D     ; Get flag/scan count
AT15:    MOV al,dh
;        ANI     0FH     ; Isolate count
         AND al,0FH
;        CPI     1       ; On first position ?
         CMP al,1
;        JRNZ    AT16    ; No - jump
         JNZ AT16
;        MOV     A,E     ; Get encountered piece type
         MOV al,dl
;        CPI     KING    ; Is it a King ?
         CMP al,KING
;        JRZ     AT30    ; Yes - jump
         JZ AT30
;AT16:   MOV     A,B     ; Get direction counter
AT16:    MOV al,ch
;        CPI     13      ; Scanning files or ranks ?
         CMP al,13
;        JRC     AT21    ; Yes - jump
         JC AT21
;        MOV     A,E     ; Get piece type
         MOV al,dl
;        CPI     BISHOP  ; Is it a Bishop ?
         CMP al,BISHOP
;        JRZ     AT30    ; Yes - jump
         JZ AT30
;        MOV     A,D     ; Get flags/scan count
         MOV al,dh
;        ANI     0FH     ; Isolate count
         AND al,0FH
;        CPI     1       ; On first position ?
         CMP al,1
;        JRNZ    AT12    ; No - jump
         JNZ AT12
;        CMP     E       ; Is it a Pawn ?
         CMP al,dl
;        JRNZ    AT12    ; No - jump
         JNZ AT12
;        LDA     P2      ; Fetch piece including color
         MOV AL,byte ptr [P2]
;X p41
;        BIT     7,A     ; Is it white ?
         MOV ah,al
         AND ah,80h
;        JRZ     AT20    ; Yes - jump
         JZ AT20
;        MOV     A,B     ; Get direction counter
         MOV al,ch
;        CPI     15      ; On a non-attacking diagonal ?
         CMP al,15
;        JRC     AT12    ; Yes - jump
         JC AT12
;        JMPR    AT30    ; Jump
         JMP AT30
;AT20:   MOV     A,B     ; Get direction counter
AT20:    MOV al,ch
;        CPI     15      ; On a non-attacking diagonal ?
         CMP al,15
;        JRNC    AT12    ; Yes - jump
         JNC AT12
;        JMPR    AT30    ; Jump
         JMP AT30
;AT21:   MOV     A,E     ; Get piece type
AT21:    MOV al,dl
;        CPI     ROOK    ; Is is a Rook ?
         CMP al,ROOK
;        JRNZ    AT12    ; No - jump
         JNZ AT12
;        JMPR    AT30    ; Jump
         JMP AT30
;AT25:   MOV     A,E     ; Get piece type
AT25:    MOV al,dl
;        CPI     KNIGHT  ; Is it a Knight ?
         CMP al,KNIGHT
;        JRNZ    AT12    ; No - jump
         JNZ AT12
;AT30:   LDA     T1      ; Attacked piece type/flag
AT30:    MOV AL,byte ptr [T1]
;        CPI     7       ; Call from POINTS ?
         CMP al,7
;        JRZ     AT31    ; Yes - jump
         JZ AT31
;        BIT     5,D     ; Is attacker opposite color ?
         MOV ah,dh
         AND ah,20h
;        JRZ     AT32    ; No - jump
         JZ AT32
;        MVI     A,1     ; Set attacker found flag
         MOV al,1
;        JMP     AT13    ; Jump
         JMP AT13
;AT31:   CALL    ATKSAV  ; Save attacker in attack list
AT31:    CALL ATKSAV
;AT32:   LDA     T2      ; Attacking piece type
AT32:    MOV AL,byte ptr [T2]
;        CPI     KING    ; Is it a King,?
         CMP al,KING
;        JZ      AT12    ; Yes - jump
         JZ AT12
;        CPI     KNIGHT  ; Is it a Knight ?
         CMP al,KNIGHT
;        JZ      AT12    ; Yes - jump
         JZ AT12
;        JMP     AT10    ; Jump
         JMP AT10
;p42

;****************************************************************
; ATTACK SAVE ROUTINE
;****************************************************************
; FUNCTION: --  To save an attacking piece value in the
;               attack list, and to increment the attack
;               count for that color piece.
;
;               The pin piece list is checked for the
;               attacking piece, and if found there, the
;               piece is not included in the attack list.
;
; CALLED BY: -- ATTACK
;
; CALLS:     -- PNCK
;
; ARGUMENTS: -- None
;****************************************************************
;ATKSAV: PUSH    B       ; Save Regs BC
ATKSAV:  PUSH ecx
;        PUSH    D       ; Save Regs DE
         PUSH edx
;        LDA     NPINS   ; Number of pinned pieces
         MOV AL,byte ptr [NPINS]
;        ANA     A       ; Any ?
         AND al,al
;        CNZ     PNCK    ; yes - check pin list
         JZ skip12
         CALL PNCK
skip12:
;        LIXD    T2      ; Init index to value table
         MOV esi,[T2]
;        LXI     H,ATKLST ; Init address of attack list
         MOV ebx,offset ATKLST
;        LXI     B,0     ; Init increment for white
         MOV ecx,0
;        LDA     P2      ; Attacking piece
         MOV AL,byte ptr [P2]
;        BIT     7,A     ; Is it white ?
         MOV ah,al
         AND ah,80h
;        JRZ     rel006  ; Yes - jump
         JZ rel006
;        MVI     C,7     ; Init increment for black
         MOV cl,7
;rel006: ANI     7       ; Attacking piece type
rel006:  AND al,7
;        MOV     E,A     ; Init increment for type
         MOV dl,al
;        BIT     7,D     ; Queen found this scan ?
         MOV ah,dh
         AND ah,80h
;        JRZ     rel007  ; No - jump
         JZ rel007
;        MVI     E,QUEEN ; Use Queen slot in attack list
         MOV dl,QUEEN
;rel007: DAD     B       ; Attack list address
rel007:  LAHF
         ADD ebx,ecx
         SAHF
;        INR     M       ; Increment list count
         INC byte ptr [ebx]
;        MVI     D,0
         MOV dh,0
;        DAD     D       ; Attack list slot address
         LAHF
         ADD ebx,edx
         SAHF
;        MOV     A,M     ; Get data already there
         MOV al,byte ptr [ebx]
;        ANI     0FH     ; Is first slot empty ?
         AND al,0FH
;        JRZ     AS20    ; Yes - jump
         JZ AS20
;        MOV     A,M     ; Get data again
         MOV al,byte ptr [ebx]
;        ANI     0F0H    ; Is second slot empty ?
         AND al,0F0H
;        JRZ     AS19    ; Yes - jump
         JZ AS19
;        INX     H       ; Increment to King slot
         LAHF
         INC ebx
         SAHF
;        JMPR    AS20    ; Jump
         JMP AS20
;AS19:   RLD             ; Temp save lower in upper
AS19:    CALL Z80_RLD
;        MOV     A,PVALUE(X) ; Get new value for attack list
         MOV al,byte ptr [esi+PVALUE]
;        RRD             ; Put in 2nd attack list slot
         CALL Z80_RRD
;        JMPR    AS25    ; Jump
         JMP AS25
;AS20:   MOV     A,PVALUE(X) ; Get new value for attack list
AS20:    MOV al,byte ptr [esi+PVALUE]
;        RLD             ; Put in 1st attack list slot
         CALL Z80_RLD
;X p43
;AS25:   POP     D       ; Restore DE regs
AS25:    POP edx
;        POP     B       ; Restore BC regs
         POP ecx
;        RET             ; Return
         RET

;**********************************************************
; PIN CHECK ROUTINE
;**********************************************************
; FUNCTION:  -- Checks to see if the attacker is in the
;               pinned piece list. If so he is not a valid
;               attacker unless the direction in which he
;               attacks is the same as the direction along
;               which he is pinned. If the piece is
;               found to be invalid as an attacker, the
;               return to the calling routine is aborted
;               and this routine returns directly to ATTACK.
;
; CALLED BY: -- ATKSAV
;
; CALLS:     -- None
;
; ARGUMENTS: -- The direction of the attack. The
;               pinned piece counnt.
;**********************************************************
;PNCK:   MOV     D,C     ; Save attack direction
PNCK:    MOV dh,cl
;        MVI     E,0     ; Clear flag
         MOV dl,0
;        MOV     C,A     ; Load pin count for search
         MOV cl,al
;        MVI     B,0
         MOV ch,0
;        LDA     M2      ; Position of piece
         MOV AL,byte ptr [M2]
;        LXI     H,PLISTA        ; Pin list address
         MOV ebx,offset PLISTA
;PC1:    CCIR            ; Search list for position
PC1:     CCIR
;        RNZ             ; Return if not found
         JZ skip13
         RET
skip13:
;        EXAF            ; Save search paramenters
         CALL Z80_EXAF
;        BIT     0,E     ; Is this the first find ?
         MOV ah,dl
         AND ah,1
;        JRNZ    PC5     ; No - jump
         JNZ PC5
;        SET     0,E     ; Set first find flag
         LAHF
         OR dl,1
         SAHF
;        PUSH    H       ; Get corresp index to dir list
         PUSH ebx
;        POP     X
         POP esi
;        MOV     A,9(X)  ; Get direction
         MOV al,byte ptr [esi+9]
;        CMP     D       ; Same as attacking direction ?
         CMP al,dh
;        JRZ     PC3     ; Yes - jump
         JZ PC3
;        NEG             ; Opposite direction ?
         NEG al
;        CMP     D       ; Same as attacking direction ?
         CMP al,dh
;        JRNZ    PC5     ; No - jump
         JNZ PC5
;PC3:    EXAF            ; Restore search parameters
PC3:     CALL Z80_EXAF
;        JPE     PC1     ; Jump if search not complete
         JPE PC1
;        RET             ; Return
         RET
;PC5:    POP     PSW     ; Abnormal exit
PC5:     POP eax
         sahf
;        POP     D       ; Restore regs.
         POP edx
;        POP     B
         POP ecx
;        RET             ; Return to ATTACK
         RET

;X p44
;**********************************************************
; PIN FIND ROUTINE
;**********************************************************
; FUNCTION: --  To produce a list of all pieces pinned
;               against the King or Queen, for both white
;               and black.
;
; CALLED BY: -- FNDMOV
;               EVAL
;
; CALLS: --     PATH
;               ATTACK
;
; ARGUMENTS: -- None
;**********************************************************
;PINFND: XRA     A       ; Zero pin count
PINFND:  XOR al,al
;        STA     NPINS
         MOV byte ptr [NPINS],al
;        LXI     D,POSK  ; Addr of King/Queen pos list
         MOV edx,offset POSK
;PF1:    LDAX    D       ; Get position of royal piece
PF1:     MOV al,[edx]
;        ANA     A       ; Is it on board ?
         AND al,al
;        JZ      PF26    ; No- jump
         JZ PF26
;        CPI     -1      ; At end of list ?
         CMP al,-1
;        RZ              ; Yes return
         JNZ skip14
         RET
skip14:
;        STA     M3      ; Save position as board index
         MOV byte ptr [M3],al
;        LIXD    M3      ; Load index to board
         MOV esi,[M3]
;        MOV     A,BOARD(X)      ; Get contents of board
         MOV al,byte ptr [esi+BOARD]
;        STA     P1      ; Save
         MOV byte ptr [P1],al
;        MVI     B,8     ; Init scan direction count
         MOV ch,8
;        XRA     A
         XOR al,al
;        STA     INDX2   ; Init direction index
         MOV byte ptr [INDX2],al
;        LIYD    INDX2
         MOV edi,[INDX2]
;PF2:    LDA     M3      ; Get King/Queen position
PF2:     MOV AL,byte ptr [M3]
;        STA     M2      ; Save
         MOV byte ptr [M2],al
;        XRA     A
         XOR al,al
;        STA     M4      ; Clear pinned piece saved pos
         MOV byte ptr [M4],al
;        MOV     C,DIRECT(Y)     ; Get direction of scan
         MOV cl,byte ptr [edi+DIRECT]
;PF5:    CALL    PATH    ; Compute next position
PF5:     CALL PATH
;        ANA     A       ; Is it empty ?
         AND al,al
;        JRZ     PF5     ; Yes - jump
         JZ PF5
;        CPI     3       ; Off board ?
         CMP al,3
;        JZ      PF25    ; Yes - jump
         JZ PF25
;        CPI     2       ; Piece of same color
         CMP al,2
;        LDA     M4      ; Load pinned piece position
         MOV AL,byte ptr [M4]
;        JRZ     PF15    ; Yes - jump
         JZ PF15
;        ANA     A       ; Possible pin ?
         AND al,al
;        JZ      PF25    ; No - jump
         JZ PF25
;        LDA     T2      ; Piece type encountered
         MOV AL,byte ptr [T2]
;        CPI     QUEEN   ; Queen ?
         CMP al,QUEEN
;        JZ      PF19    ; Yes - jump
         JZ PF19
;        MOV     L,A     ; Save piece type
         MOV bl,al
;X p45
;        MOV     A,B     ; Direction counter
         MOV al,ch
;        CPI     5       ; Non-diagonal direction ?
         CMP al,5
;        JRC     PF10    ; Yes - jump
         JC PF10
;        MOV     A,L     ; Piece type
         MOV al,bl
;        CPI     BISHOP  ; Bishop ?
         CMP al,BISHOP
;        JNZ     PF25    ; No - jump
         JNZ PF25
;        JMP     PF20    ; Jump
         JMP PF20
;PF10:   MOV     A,L     ; Piece type
PF10:    MOV al,bl
;        CPI     ROOK    ; Rook ?
         CMP al,ROOK
;        JNZ     PF25    ; No - jump
         JNZ PF25
;        JMP     PF20    ; Jump
         JMP PF20
;PF15:   ANA     A       ; Possible pin ?
PF15:    AND al,al
;        JNZ     PF25    ; No - jump
         JNZ PF25
;        LDA     M2      ; Save possible pin position
         MOV AL,byte ptr [M2]
;        STA     M4
         MOV byte ptr [M4],al
;        JMP     PF5     ; Jump
         JMP PF5
;PF19:   LDA     P1      ; Load King or Queen
PF19:    MOV AL,byte ptr [P1]
;        ANI     7       ; Clear flags
         AND al,7
;        CPI     QUEEN   ; Queen ?
         CMP al,QUEEN
;        JRNZ    PF20    ; No - jump
         JNZ PF20
;        PUSH    B       ; Save regs.
         PUSH ecx
;        PUSH    D
         PUSH edx
;        PUSH    Y
         PUSH edi
;        XRA     A       ; Zero out attack list
         XOR al,al
;        MVI     B,14
         MOV ch,14
;        LXI     H,ATKLST
         MOV ebx,offset ATKLST
;back02: MOV     M,A
back02:  MOV byte ptr [ebx],al
;        INX     H
         LAHF
         INC ebx
         SAHF
;        DJNZ    back02
         LAHF
         DEC ch
         JNZ back02
         SAHF
;        MVI     A,7     ; Set attack flag
         MOV al,7
;        STA     T1
         MOV byte ptr [T1],al
;        CALL    ATTACK  ; Find attackers/defenders
         CALL ATTACK
;        LXI     H,WACT  ; White queen attackers
         MOV ebx,WACT
;        LXI     D,BACT  ; Black queen attackers
         MOV edx,BACT
;        LDA     P1      ; Get queen
         MOV AL,byte ptr [P1]
;        BIT     7,A     ; Is she white ?
         MOV ah,al
         AND ah,80h
;        JRZ     rel008  ; Yes - skip
         JZ rel008
;        XCHG            ; Reverse for black
         XCHG ebx,edx
;rel008: MOV     A,M     ; Number of defenders
rel008:  MOV al,byte ptr [ebx]
;        XCHG            ; Reverse for attackers
         XCHG ebx,edx
;        SUB     M       ; Defenders minus attackers
         SUB al,byte ptr [ebx]
;        DCR     A       ; Less 1
         DEC al
;        POP     Y       ; Restore regs.
         POP edi
;        POP     D
         POP edx
;        POP     B
         POP ecx
;        JP      PF25    ; Jump if pin not valid
         JP PF25
;PF20:   LXI     H,NPINS ; Address of pinned piece count
PF20:    MOV ebx,offset NPINS
;        INR     M       ; Increment
         INC byte ptr [ebx]
;        LIXD    NPINS   ; Load pin list index
         MOV esi,[NPINS]
;        MOV     PLISTD(X),C     ; Save direction of pin
         MOV byte ptr [esi+PLISTD],cl
;X p46
;        LDA     M4      ; Position of pinned piece
         MOV AL,byte ptr [M4]
;        MOV     PLIST(X),A      ; Save in list
         MOV byte ptr [esi+PLIST],al
;PF25:   INX     Y       ; Increment direction index
PF25:    LAHF
         INC edi
         SAHF
;        DJNZ    PF27    ; Done ? No - Jump
         LAHF
         DEC ch
         JNZ PF27
         SAHF
;PF26:   INX     D       ; Incr King/Queen pos index
PF26:    LAHF
         INC edx
         SAHF
;        JMP     PF1     ; Jump
         JMP PF1
;PF27:   JMP     PF2     ; Jump
PF27:    JMP PF2

;X p47
;****************************************************************
; EXCHANGE ROUTINE
;****************************************************************
; FUNCTION: --  To determine the exchange value of a
;               piece on a given square by examining all
;               attackers and defenders of that piece.
;
; CALLED BY: -- POINTS
;
; CALLS: --     NEXTAD
;
; ARGUMENTS: -- None.
;****************************************************************
;XCHNG:  EXX             ; Swap regs.
XCHNG:   CALL Z80_EXX
;        LDA     P1      ; Piece attacked
         MOV AL,byte ptr [P1]
;        LXI     H,WACT  ; Addr of white attkrs/dfndrs
         MOV ebx,WACT
;        LXI     D,BACT  ; Addr of black attkrs/dfndrs
         MOV edx,BACT
;        BIT     7,A     ; Is piece white ?
         MOV ah,al
         AND ah,80h
;        JRZ     rel009  ; Yes - jump
         JZ rel009
;        XCHG            ; Swap list pointers
         XCHG ebx,edx
;rel009: MOV     B,M     ; Init list counts
rel009:  MOV ch,byte ptr [ebx]
;        XCHG
         XCHG ebx,edx
;        MOV     C,M
         MOV cl,byte ptr [ebx]
;        XCHG
         XCHG ebx,edx
;        EXX             ; Restore regs.
         CALL Z80_EXX
;        MVI     C,0     ; Init attacker/defender flag
         MOV cl,0
;        MVI     E,0     ; Init points lost count
         MOV dl,0
;        LIXD    T3      ; Load piece value index
         MOV esi,[T3]
;        MOV     D,PVALUE(X)     ; Get attacked piece value
         MOV dh,byte ptr [esi+PVALUE]
;        SLAR    D       ; Double it
         SHL dh,1
;        MOV     B,D     ; Save
         MOV ch,dh
;        CALL    NEXTAD  ; Retrieve first attacker
         CALL NEXTAD
;        RZ              ; Return if none
         JNZ skip15
         RET
skip15:
;XC10:   MOV     L,A     ; Save attacker value
XC10:    MOV bl,al
;        CALL    NEXTAD  ; Get next defender
         CALL NEXTAD
;        JRZ     XC18    ; Jump if none
         JZ XC18
;        EXAF            ; Save defender value
         CALL Z80_EXAF
;        MOV     A,B     ; Get attacked value
         MOV al,ch
;        CMP     L       ; Attacked less than attacker ?
         CMP al,bl
;        JRNC    XC19    ; No - jump
         JNC XC19
;        EXAF            ; -Restore defender
         CALL Z80_EXAF
;XC15:   CMP     L       ; Defender less than attacker ?
XC15:    CMP al,bl
;        RC              ; Yes - return
         JNC skip16
         RET
skip16:
;        CALL    NEXTAD  ; Retrieve next attacker value
         CALL NEXTAD
;        RZ              ; Return if none
         JNZ skip17
         RET
skip17:
;        MOV     L,A     ; Save attacker value
         MOV bl,al
;        CALL    NEXTAD  ; Retrieve next defender value
         CALL NEXTAD
;        JRNZ    XC15    ; Jump if none
         JNZ XC15
;XC18:   EXAF            ; Save Defender
XC18:    CALL Z80_EXAF
;        MOV     A,B     ; Get value of attacked piece
         MOV al,ch
;X p48
;XC19:   BIT     0,C     ; Attacker or defender ?
XC19:    MOV ah,cl
         AND ah,1
;        JRZ     rel010  ; Jump if defender
         JZ rel010
;        NEG             ; Negate value for attacker
         NEG al
;        ADD     E       ; Total points lost
         ADD al,dl
;rel010: MOV     E,A     ; Save total
rel010:  MOV dl,al
;        EXAF            ; Restore previous defender
         CALL Z80_EXAF
;        RZ              ; Return if none
         JNZ skip18
         RET
skip18:
;        MOV     B,L     ; Prev attckr becomes defender
         MOV ch,bl
;        JMP     XC10    ; Jump
         JMP XC10

;****************************************************************
; NEXT ATTACKER/DEFENDER ROUTINE
;****************************************************************
; FUNCTION: --  To retrieve the next attacker or defender
;               piece value from the attack list, and delete
;               that piece from the list.
;
; CALLED BY: -- XCHNG
;
; CALLS: --     None
;
; ARGUMENTS: -- Attack list addresses.
;               Side flag
;               Attack list counts
;****************************************************************
;NEXTAD: INR     C       ; Increment side flag
NEXTAD:  INC cl
;        EXX             ; Swap registers
         CALL Z80_EXX
;        MOV     A,B     ; Swap list counts
         MOV al,ch
;        MOV     B,C
         MOV ch,cl
;        MOV     C,A
         MOV cl,al
;        XCHG            ; Swap list pointers
         XCHG ebx,edx
;        XRA     A
         XOR al,al
;        CMP     B       ; At end of list ?
         CMP al,ch
;        JRZ     NX6     ; Yes - jump
         JZ NX6
;        DCR     B       ; Decrement list count
         DEC ch
;back03: INX     H       ; Increment list inter
back03:  LAHF
         INC ebx
         SAHF
;        CMP     M       ; Check next item in list
         CMP al,byte ptr [ebx]
;        JRZ     back03  ; Jump if empty
         JZ back03
;        RRD             ; Get value from list
         CALL Z80_RRD
;        ADD     A       ; Double it
         ADD al,al
;        DCX     H       ; Decrement list pointer
         LAHF
         DEC ebx
         SAHF
;NX6:    EXX             ; Restore regs.
NX6:     CALL Z80_EXX
;        RET             ; Return
         RET

;X p49
;****************************************************************
; POINT EVALUATION ROUTINE
;****************************************************************
; FUNCTION: --  To perform a static board evaluation and
;               derive a score for a given board position
;
; CALLED BY: -- FNDMOV
;               EVAL
;
; CALLS: --     ATTACK
;               XCHNG
;               LIMIT
;
; ARGUMENTS: -- None
;****************************************************************
;POINTS: XRA     A       ; Zero out variables
POINTS:  XOR al,al
;        STA     MTRL
         MOV byte ptr [MTRL],al
;        STA     BRDC
         MOV byte ptr [BRDC],al
;        STA     PTSL
         MOV byte ptr [PTSL],al
;        STA     PTSW1
         MOV byte ptr [PTSW1],al
;        STA     PTSW2
         MOV byte ptr [PTSW2],al
;        STA     PTSCK
         MOV byte ptr [PTSCK],al
;        LXI     H,T1    ; Set attacker flag
         MOV ebx,offset T1
;        MVI     M,7
         MOV byte ptr [ebx],7
;        MVI     A,21    ; Init to first square on board
         MOV al,21
;PT5:    STA     M3      ; Save as board index
PT5:     MOV byte ptr [M3],al
;        LIXD    M3      ; Load board index
         MOV esi,[M3]
;        MOV     A,BOARD(X)      ; Get piece from board
         MOV al,byte ptr [esi+BOARD]
;        CPI     -1      ; Off board edge ?
         CMP al,-1
;        JZ      PT25    ; Yes - jump
         JZ PT25
;        LXI     H,P1    ; Save piece, if any
         MOV ebx,offset P1
;        MOV     M,A
         MOV byte ptr [ebx],al
;        ANI     7       ; Save piece type, if any
         AND al,7
;        STA     T3
         MOV byte ptr [T3],al
;        CPI     KNIGHT  ; Less than a Knight (Pawn) ?
         CMP al,KNIGHT
;        JRC     PT6X    ; Yes - Jump
         JC PT6X
;        CPI     ROOK    ; Rook, Queen or King ?
         CMP al,ROOK
;        JRC     PT6B    ; No - jump
         JC PT6B
;        CPI     KING    ; Is it a King ?
         CMP al,KING
;        JRZ     PT6AA   ; Yes - jump
         JZ PT6AA
;        LDA     MOVENO  ; Get move number
         MOV AL,byte ptr [MOVENO]
;        CPI     7       ; Less than 7 ?
         CMP al,7
;        JRC     PT6A    ; Yes - Jump
         JC PT6A
;        JMP     PT6X    ; Jump
         JMP PT6X
;PT6AA:  BIT     4,M     ; Castled yet ?
PT6AA:   MOV ah,byte ptr [ebx]
         AND ah,10h
;        JRZ     PT6A    ; No - jump
         JZ PT6A
;        MVI     A,+6    ; Bonus for castling
         MOV al,+6
;        BIT     7,M     ; Check piece color
         MOV ah,byte ptr [ebx]
         AND ah,80h
;        JRZ     PT6D    ; Jump if white
         JZ PT6D
;        MVI     A,-6    ; Bonus for black castling
         MOV al,-6
;X p50
;        JMP     PT6D    ; Jump
         JMP PT6D
;PT6A:   BIT     3,M     ; Has piece moved yet ?
PT6A:    MOV ah,byte ptr [ebx]
         AND ah,8
;        JRZ     PT6X    ; No - jump
         JZ PT6X
;        JMP     PT6C    ; Jump
         JMP PT6C
;PT6B:   BIT     3,M     ; Has piece moved yet ?
PT6B:    MOV ah,byte ptr [ebx]
         AND ah,8
;        JRNZ    PT6X    ; Yes - jump
         JNZ PT6X
;PT6C:   MVI     A,-2    ; Two point penalty for white
PT6C:    MOV al,-2
;        BIT     7,M     ; Check piece color
         MOV ah,byte ptr [ebx]
         AND ah,80h
;        JRZ     PT6D    ; Jump if white
         JZ PT6D
;        MVI     A,+2    ; Two point penalty for black
         MOV al,+2
;PT6D:   LXI     H,BRDC  ; Get address of board control
PT6D:    MOV ebx,offset BRDC
;        ADD     M       ; Add on penalty/bonus points
         ADD al,byte ptr [ebx]
;        MOV     M,A     ; Save
         MOV byte ptr [ebx],al
;PT6X:   XRA     A       ; Zero out attack list
PT6X:    XOR al,al
;        MVI     B,14
         MOV ch,14
;        LXI     H,ATKLST
         MOV ebx,offset ATKLST
;back04: MOV     M,A
back04:  MOV byte ptr [ebx],al
;        INX     H
         LAHF
         INC ebx
         SAHF
;        DJNZ    back04
         LAHF
         DEC ch
         JNZ back04
         SAHF
;        CALL    ATTACK  ; Build attack list for square
         CALL ATTACK
;        LXI     H,BACT  ; Get black attacker count addr
         MOV ebx,BACT
;        LDA     WACT    ; Get white attacker count
         MOV AL,byte ptr [WACT]
;        SUB     M       ; Compute count difference
         SUB al,byte ptr [ebx]
;        LXI     H,BRDC  ; Address of board control
         MOV ebx,offset BRDC
;        ADD     M       ; Accum board control score
         ADD al,byte ptr [ebx]
;        MOV     M,A     ; Save
         MOV byte ptr [ebx],al
;        LDA     P1      ; Get piece on current square
         MOV AL,byte ptr [P1]
;        ANA     A       ; Is it empty ?
         AND al,al
;        JZ      PT25    ; Yes - jump
         JZ PT25
;        CALL    XCHNG   ; Evaluate exchange, if any
         CALL XCHNG
;        XRA     A       ; Check for a loss
         XOR al,al
;        CMP     E       ; Points lost ?
         CMP al,dl
;        JRZ     PT23    ; No - Jump
         JZ PT23
;        DCR     D       ; Deduct half a Pawn value
         DEC dh
;        LDA     P1      ; Get piece under attack
         MOV AL,byte ptr [P1]
;        LXI     H,COLOR ; Color of side just moved
         MOV ebx,offset COLOR
;        XRA     M       ; Compare with piece
         XOR al,byte ptr [ebx]
;        BIT     7,A     ; Do colors match ?
         MOV ah,al
         AND ah,80h
;        MOV     A,E     ; Points lost
         MOV al,dl
;        JRNZ    PT20    ; Jump if no match
         JNZ PT20
;        LXI     H,PTSL  ; Previous max points lost
         MOV ebx,offset PTSL
;        CMP     M       ; Compare to current value
         CMP al,byte ptr [ebx]
;        JRC     PT23    ; Jump if greater than
         JC PT23
;        MOV     M,E     ; Store new value as max lost
         MOV byte ptr [ebx],dl
;        LIXD    MLPTRJ  ; Load pointer to this move
         MOV esi,[MLPTRJ]
;        LDA     M3      ; Get position of lost piece
         MOV AL,byte ptr [M3]
;        CMP     MLTOP(X)        ; Is it the one moving ?
         CMP al,byte ptr [esi+MLTOP]
;        JRNZ    PT23    ; No - jump
         JNZ PT23
;        STA     PTSCK   ; Save position as a flag
         MOV byte ptr [PTSCK],al
;        JMP     PT23    ; Jump
         JMP PT23
;X p51
;PT20:   LXI     H,PTSW1 ; Previous maximum points won
PT20:    MOV ebx,offset PTSW1
;        CMP     M       ; Compare to current value
         CMP al,byte ptr [ebx]
;        JRC     rel011  ; Jump if greater than
         JC rel011
;        MOV     A,M     ; Load previous max value
         MOV al,byte ptr [ebx]
;        MOV     M,E     ; Store new value as max won
         MOV byte ptr [ebx],dl
;rel011: LXI     H,PTSW2 ; Previous 2nd max points won
rel011:  MOV ebx,offset PTSW2
;        CMP     M       ; Compare to current value
         CMP al,byte ptr [ebx]
;        JRC     PT23    ; Jump if greater than
         JC PT23
;        MOV     M,A     ; Store as new 2nd max lost
         MOV byte ptr [ebx],al
;PT23:   LXI     H,P1    ; Get piece
PT23:    MOV ebx,offset P1
;        BIT     7,M     ; Test color
         MOV ah,byte ptr [ebx]
         AND ah,80h
;        MOV     A,D     ; Value of piece
         MOV al,dh
;        JRZ     rel012  ; Jump if white
         JZ rel012
;        NEG             ; Negate for black
         NEG al
;rel012: LXI     H,MTRL  ; Get addrs of material total
rel012:  MOV ebx,offset MTRL
;        ADD     M       ; Add new value
         ADD al,byte ptr [ebx]
;        MOV     M,A     ; Store
         MOV byte ptr [ebx],al
;PT25:   LDA     M3      ; Get current board position
PT25:    MOV AL,byte ptr [M3]
;        INR     A       ; Increment
         INC al
;        CPI     99      ; At end of board ?
         CMP al,99
;        JNZ     PT5     ; No - jump
         JNZ PT5
;        LDA     PTSCK   ; Moving piece lost flag
         MOV AL,byte ptr [PTSCK]
;        ANA     A       ; Was it lost ?
         AND al,al
;        JRZ     PT25A   ; No - jump
         JZ PT25A
;        LDA     PTSW2   ; 2nd max points won
         MOV AL,byte ptr [PTSW2]
;        STA     PTSW1   ; Store as max points won
         MOV byte ptr [PTSW1],al
;        XRA     A       ; Zero out 2nd max points won
         XOR al,al
;        STA     PTSW2
         MOV byte ptr [PTSW2],al
;PT25A:  LDA     PTSL    ; Get max points lost
PT25A:   MOV AL,byte ptr [PTSL]
;        ANA     A       ; Is it zero ?
         AND al,al
;        JRZ     rel013  ; Yes - jump
         JZ rel013
;        DCR     A       ; Decrement it
         DEC al
;rel013: MOV     B,A     ; Save it
rel013:  MOV ch,al
;        LDA     PTSW1   ; Max,points won
         MOV AL,byte ptr [PTSW1]
;        ANA     A       ; Is it zero ?
         AND al,al
;        JRZ     rel014  ; Yes - jump
         JZ rel014
;        LDA     PTSW2   ; 2nd max points won
         MOV AL,byte ptr [PTSW2]
;        ANA     A       ; Is it zero ?
         AND al,al
;        JRZ     rel014  ; Yes - jump
         JZ rel014
;        DCR     A       ; Decrement it
         DEC al
;        SRLR    A       ; Divide it by 2
         SHR al,1
;        SUB     B       ; Subtract points lost
         SUB al,ch
;rel014: LXI     H,COLOR ; Color of side just moved ???
rel014:  MOV ebx,offset COLOR
;        BIT     7,M     ; Is it white ?
         MOV ah,byte ptr [ebx]
         AND ah,80h
;        JRZ     rel015  ; Yes - jump
         JZ rel015
;        NEG             ; Negate for black
         NEG al
;rel015: LXI     H,MTRL  ; Net material on board
rel015:  MOV ebx,offset MTRL
;        ADD     M       ; Add exchange adjustments
         ADD al,byte ptr [ebx]
;        LXI     H,MV0   ; Material at ply 0
         MOV ebx,offset MV0
;X p52
;        SUB     M       ; Subtract from current
         SUB al,byte ptr [ebx]
;        MOV     B,A     ; Save
         MOV ch,al
;        MVI     A,30    ; Load material limit
         MOV al,30
;        CALL    LIMIT   ; Limit to plus or minus value
         CALL LIMIT
;        MOV     E,A     ; Save limited value
         MOV dl,al
;        LDA     BRDC    ; Get board control points
         MOV AL,byte ptr [BRDC]
;        LXI     H,BC0   ; Board control at ply zero
         MOV ebx,offset BC0
;        SUB     M       ; Get difference
         SUB al,byte ptr [ebx]
;        MOV     B,A     ; Save
         MOV ch,al
;        LDA     PTSCK   ; Moving piece lost flag
         MOV AL,byte ptr [PTSCK]
;        ANA     A       ; Is it zero ?
         AND al,al
;        JRZ     rel026  ; Yes - jump
         JZ rel026
;        MVI     B,0     ; Zero board control points
         MOV ch,0
;rel026: MVI     A,6     ; Load board control limit
rel026:  MOV al,6
;        CALL    LIMIT   ; Limit to plus or minus value
         CALL LIMIT
;        MOV     D,A     ; Save limited value
         MOV dh,al
;        MOV     A,E     ; Get material points
         MOV al,dl
;        ADD     A       ; Multiply by 4
         ADD al,al
;        ADD     A
         ADD al,al
;        ADD     D       ; Add board control
         ADD al,dh
;        LXI     H,COLOR ; Color of side just moved
         MOV ebx,offset COLOR
;        BIT     7,M     ; Is it white ?
         MOV ah,byte ptr [ebx]
         AND ah,80h
;        JRNZ    rel016  ; No - jump
         JNZ rel016
;        NEG             ; Negate for white
         NEG al
;rel016: ADI     80H     ; Rescale score (neutral = 80H
rel016:  ADD al,80H
;        STA     VALM    ; Save score
         MOV byte ptr [VALM],al
;        LIXD    MLPTRJ  ; Load move list pointer
         MOV esi,[MLPTRJ]
;        MOV     MLVAL(X),A      ; Save score in move list
         MOV byte ptr [esi+MLVAL],al
;        RET             ; Return
         RET

;X p53
;**********************************************************
; LIMIT ROUTINE
;**********************************************************
; FUNCTION: --  To limit the magnitude of a given value
;               to another given value.
;
; CALLED BY: -- POINTS
;
; CALLS: --     None
;
; ARGUMENTS: -- Input   - Value, to be limited in the B
;                         register.
;                       - Value to limit to in the A register
;               Output  - Limited value in the A register.
;**********************************************************
;LIMIT:  BIT     7,B     ; Is value negative ?
LIMIT:   MOV ah,ch
         AND ah,80h
;        JZ      LIM10   ; No - jump
         JZ LIM10
;        NEG             ; Make positive
         NEG al
;        CMP     B       ; Compare to limit
         CMP al,ch
;        RNC             ; Return if outside limit
         JC skip19
         RET
skip19:
;        MOV     A,B     ; Output value as is
         MOV al,ch
;        RET             ; Return
         RET
;LIM10:  CMP     B       ; Compare to limit
LIM10:   CMP al,ch
;        RC              ; Return if outside limit
         JNC skip20
         RET
skip20:
;        MOV     A,B     ; Output value as is
         MOV al,ch
;        RET             ; Return
         RET
;X      .END            ;X Bug in the original listing.

;X p54
;**********************************************************
; MOVE ROUTINE
;**********************************************************
; FUNCTION: --  To execute a move from the move list on the
;               board array.
;
; CALLED BY: -- CPTRMV
;               PLYRMV
;               EVAL
;               FNDMOV
;               VALMOV
;
; CALLS: --     None
;
; ARGUMENTS: -- None
;**********************************************************
;MOVE:   LHLD    MLPTRJ  ; Load move list pointer
MOVE:    MOV ebx,[MLPTRJ]
;        INX     H       ; Increment past link bytes
         LAHF
         INC ebx
         SAHF
;        INX     H
         LAHF
         INC ebx
         SAHF
;        INX     H
         LAHF
         INC ebx
         SAHF
;        INX     H
         LAHF
         INC ebx
         SAHF
;MV1:    MOV     A,M     ; "From" position
MV1:     MOV al,byte ptr [ebx]
;        STA     M1      ; Save
         MOV byte ptr [M1],al
;        INX     H       ; Increment pointer
         LAHF
         INC ebx
         SAHF
;        MOV     A,M     ; "To" position
         MOV al,byte ptr [ebx]
;        STA     M2      ; Save
         MOV byte ptr [M2],al
;        INX     H       ; Increment pointer
         LAHF
         INC ebx
         SAHF
;        MOV     D,M     ; Get captured piece/flags
         MOV dh,byte ptr [ebx]
;        LIXD    M1      ; Load "from" pos board index
         MOV esi,[M1]
;        MOV     E,BOARD(X)      ; Get piece moved
         MOV dl,byte ptr [esi+BOARD]
;        BIT     5,D     ; Test Pawn promotion flag
         MOV ah,dh
         AND ah,20h
;        JRNZ    MV15    ; Jump if set
         JNZ MV15
;        MOV     A,E     ; Piece moved
         MOV al,dl
;        ANI     7       ; Clear flag bits
         AND al,7
;        CPI     QUEEN   ; Is it a queen ?
         CMP al,QUEEN
;        JRZ     MV20    ; Yes - jump
         JZ MV20
;        CPI     KING    ; Is it a king ?
         CMP al,KING
;        JRZ     MV30    ; Yes - jump
         JZ MV30
;MV5:    LIYD    M2      ; Load "to" pos board index
MV5:     MOV edi,[M2]
;        SET     3,E     ; Set piece moved flag
         LAHF
         OR dl,8
         SAHF
;        MOV     BOARD(Y),E      ; Insert piece at new position
         MOV byte ptr [edi+BOARD],dl
;        MVI     BOARD(X),0      ; Empty previous position
         MOV byte ptr [esi+BOARD],0
;        BIT     6,D     ; Double move ?
         MOV ah,dh
         AND ah,40h
;        JRNZ    MV40    ; Yes - jump
         JNZ MV40
;        MOV     A,D     ; Get captured piece, if any
         MOV al,dh
;        ANI     7
         AND al,7
;        CPI     QUEEN   ; Was it a queen ?
         CMP al,QUEEN
;        RNZ             ; No - return
         JZ skip21
         RET
skip21:
;        LXI     H,POSQ  ; Addr of saved Queen position
         MOV ebx,offset POSQ
;        BIT     7,D     ; Is Queen white ?
         MOV ah,dh
         AND ah,80h
;        JRZ     MV10    ; Yes - jump
         JZ MV10
;        INX     H       ; Increment to black Queen pos
         LAHF
         INC ebx
         SAHF
;X p55
;MV10:   XRA     A       ; Set saved position to zero
MV10:    XOR al,al
;        MOV     M,A
         MOV byte ptr [ebx],al
;        RET             ; Return
         RET
;MV15:   SET     2,E     ; Change Pawn to a Queen
MV15:    LAHF
         OR dl,4
         SAHF
;        JMP     MV5     ; Jump
         JMP MV5
;MV20:   LXI     H,POSQ  ; Addr of saved Queen position
MV20:    MOV ebx,offset POSQ
;MV21:   BIT     7,E     ; Is Queen white ?
MV21:    MOV ah,dl
         AND ah,80h
;        JRZ     MV22    ; Yes - jump
         JZ MV22
;        INX     H
         LAHF
         INC ebx
         SAHF
;MV22:   LDA     M2      ; Get rnewnQueenbpositionen pos
MV22:    MOV AL,byte ptr [M2]
;        MOV     M,A     ; Save
         MOV byte ptr [ebx],al
;        JMP     MV5     ; Jump
         JMP MV5
;MV30:   LXI     H,POSK  ; Get saved King position
MV30:    MOV ebx,offset POSK
;        BIT     6,D     ; Castling ?
         MOV ah,dh
         AND ah,40h
;        JRZ     MV21    ; No - jump
         JZ MV21
;        SET     4,E     ; Set King castled flag
         LAHF
         OR dl,10h
         SAHF
;        JMP     MV21    ; Jump
         JMP MV21
;MV40:   LHLD    MLPTRJ  ; Get move list pointer
MV40:    MOV ebx,[MLPTRJ]
;        LXI     D,8     ; Increment to next move
         MOV edx,8
;        DAD     D
         LAHF
         ADD ebx,edx
         SAHF
;        JMP     MV1     ; Jump (2nd part of dbl move)
         JMP MV1

;X p56
;**********************************************************
; UN-MOVE ROUTINE
;**********************************************************
; FUNCTION: --  To reverse the process of the move routine,
;               thereby restoring the board array to its
;               previous position.
;
; CALLED BY: -- VALMOV
;               EVAL
;               FNDMOV
;               ASCEND
;
; CALLS: --     None
;
; ARGUMENTS: -- None
;**********************************************************
;UNMOVE: LHLD    MLPTRJ  ; Load move list pointer
UNMOVE:  MOV ebx,[MLPTRJ]
;        INX     H       ; Increment past link bytes
         add bx,4
;UM1:    MOV     A,M     ; Get "from" position
UM1:     MOV al,byte ptr [ebx]
;        STA     M1      ; Save
         MOV byte ptr [M1],al
;        INX     H       ; Increment pointer
         LAHF
         INC ebx
         SAHF
;        MOV     A,M     ; Get "to" position
         MOV al,byte ptr [ebx]
;        STA     M2      ; Save
         MOV byte ptr [M2],al
;        INX     H       ; Increment pointer
         LAHF
         INC ebx
         SAHF
;        MOV     D,M     ; Get captured piece/flags
         MOV dh,byte ptr [ebx]
;        LIXD    M2      ; Load "to" pos board index
         MOV esi,[M2]
;        MOV     E,BOARD(X)      ; Get piece moved
         MOV dl,byte ptr [esi+BOARD]
;        BIT     5,D     ; Was it a Pawn promotion ?
         MOV ah,dh
         AND ah,20h
;        JRNZ    UM15    ; Yes - jump
         JNZ UM15
;        MOV     A,E     ; Get piece moved
         MOV al,dl
;        ANI     7       ; Clear flag bits
         AND al,7
;        CPI     QUEEN   ; Was it a Queen ?
         CMP al,QUEEN
;        JRZ     UM20    ; Yes - jump
         JZ UM20
;        CPI     KING    ; Was it a King ?
         CMP al,KING
;        JRZ     UM30    ; Yes - jump
         JZ UM30
;UM5:    BIT     4,D     ; Is this 1st move for piece ?
UM5:     MOV ah,dh
         AND ah,10h
;        JRNZ    UM16    ; Yes - jump
         JNZ UM16
;UM6:    LIYD    M1      ; Load "from" pos board index
UM6:     MOV edi,[M1]
;        MOV     BOARD(Y),E      ; Return to previous board pos
         MOV byte ptr [edi+BOARD],dl
;        MOV     A,D     ; Get captured piece, if any
         MOV al,dh
;        ANI     8FH     ; Clear flags
         AND al,8FH
;        MOV     BOARD(X),A      ; Return to board
         MOV byte ptr [esi+BOARD],al
;        BIT     6,D     ; Was it a double move ?
         MOV ah,dh
         AND ah,40h
;        JRNZ    UM40    ; Yes - jump
         JNZ UM40
;        MOV     A,D     ; Get captured piece, if any
         MOV al,dh
;        ANI     7       ; Clear flag bits
         AND al,7
;        CPI     QUEEN   ; Was it a Queen ?
         CMP al,QUEEN
;        RNZ             ; No - return
         JZ skip22
         RET
skip22:
;X p57
;        LXI     H,POSQ  ; Address of saved Queen pos
         MOV ebx,offset POSQ
;        BIT     7,D     ; Is Queen white ?
         MOV ah,dh
         AND ah,80h
;        JRZ     UM10    ; Yes - jump
         JZ UM10
;        INX     H       ; Increment to black Queen pos
         LAHF
         INC ebx
         SAHF
;UM10:   LDA     M2      ; Queen's previous position
UM10:    MOV AL,byte ptr [M2]
;        MOV     M,A     ; Save
         MOV byte ptr [ebx],al
;        RET             ; Return
         RET
;UM15:   RES     2,E     ; Restore Queen to Pawn
UM15:    LAHF
         AND dl,0fbh
         SAHF
;        JMP     UM5     ; Jump
         JMP UM5
;UM16:   RES     3,E     ; Clear piece moved flag
UM16:    LAHF
         AND dl,0f7h
         SAHF
;        JMP     UM6     ; Jump
         JMP UM6
;UM20:   LXI     H,POSQ  ; Addr of saved Queen position
UM20:    MOV ebx,offset POSQ
;UM21:   BIT     7,E     ; Is Queen white ?
UM21:    MOV ah,dl
         AND ah,80h
;        JRZ     UM22    ; Yes - jump
         JZ UM22
;        INX     H       ; Increment to black Queen pos
         LAHF
         INC ebx
         SAHF
;UM22:   LDA     M1      ; Get previous position
UM22:    MOV AL,byte ptr [M1]
;        MOV     M,A     ; Save
         MOV byte ptr [ebx],al
;        JMP     UM5     ; Jump
         JMP UM5
;UM30:   LXI     H,POSK  ; Address of saved King pos
UM30:    MOV ebx,offset POSK
;        BIT     6,D     ; Was it a castle ?
         MOV ah,dh
         AND ah,40h
;        JRZ     UM21    ; No - jump
         JZ UM21
;        RES     4,E     ; Clear castled flag
         LAHF
         AND dl,0efh
         SAHF
;        JMP     UM21    ; Jump
         JMP UM21
;UM40:   LHLD    MLPTRJ  ; Load move list pointer
UM40:    MOV ebx,[MLPTRJ]
;        LXI     D,8     ; Increment to next move
         MOV edx,8
;        DAD     D
         LAHF
         ADD ebx,edx
         SAHF
;        JMP     UM1     ; Jump (2nd part of dbl move)
         JMP UM1

;X p58
;***********************************************************
; SORT ROUTINE
;***********************************************************
; FUNCTION: --  To sort the move list in order of
;               increasing move value scores.
;
; CALLED BY: -- FNDMOV
;
; CALLS: --     EVAL
;
; ARGUMENTS: -- None
;***********************************************************
;SORTM:  LBCD    MLPTRI  ; Move list begin pointer
SORTM:   MOV ecx,[MLPTRI]
;        LXI     D,0     ; Initialize working pointers
         MOV edx,0
SR5:     mov ebx,ecx
;;SR5:    MOV     H,B
;SR5:     MOV bh,ch
;;        MOV     L,C
;         ;MOV bl,cl

         mov ecx,[ebx]
;;        MOV     C,M     ; Link to next move
;         MOV cl,byte ptr [ebx]
;;        INX     H
;         LAHF
;         INC ebx
;         SAHF
;;        MOV     B,M
;         MOV ch,byte ptr [ebx]

        mov [ebx],edx
;        MOV     M,D     ; Store to link in list
;         MOV byte ptr [ebx],dh
;        DCX     H
;         LAHF
;         DEC ebx
;         SAHF
;        MOV     M,E
;         MOV byte ptr [ebx],dl

        cmp ecx,0
;;        XRA     A       ; End of list ?
;         XOR al,al
;;        CMP     B
;         CMP al,ch

;        RZ              ; Yes - return
         JNZ skip23
         RET
skip23:
;SR10:   SBCD    MLPTRJ  ; Save list pointer
SR10:    MOV [MLPTRJ],ecx
;        CALL    EVAL    ; Evaluate move
         CALL EVAL
;        LHLD    MLPTRI  ; Begining of move list
         MOV ebx,[MLPTRI]
;        LBCD    MLPTRJ  ; Restore list pointer
         MOV ecx,[MLPTRJ]

SR15:    mov edx,[ebx]
         cmp edx,0
;;SR15:   MOV     E,M     ; Next move for compare
;SR15:    MOV dl,byte ptr [ebx]
;;        INX     H
;         LAHF
;         INC ebx
;;         SAHF
;;        MOV     D,M
;         MOV dh,byte ptr [ebx]
;;        XRA     A       ; At end of list ?
;         XOR al,al
;;        CMP     D
;         CMP al,dh

;        JRZ     SR25    ; Yes - jump
         JZ SR25
;        PUSH    D       ; Transfer move pointer
         PUSH edx
;        POP     X
         POP esi
;        LDA     VALM    ; Get new move value
         MOV AL,byte ptr [VALM]
;        CMP     MLVAL(X)        ; Less than list value ?
         CMP al,byte ptr [esi+MLVAL]
;        JRNC    SR30    ; No - jump
         JNC SR30

SR25:     mov [ebx],ecx
;;SR25:   MOV     M,B     ; Link new move into list
;SR25:    MOV byte ptr [ebx],ch
;;        DCX     H
;         LAHF
;         DEC ebx
;         SAHF
;;        MOV     M,C
;         MOV byte ptr [ebx],cl

;        JMP     SR5     ; Jump
         JMP SR5
;SR30:   XCHG            ; Swap pointers
SR30:    XCHG ebx,edx
;        JMP     SR15    ; Jump
         JMP SR15

;X p59
;**********************************************************
; EVALUATION ROUTINE
;**********************************************************
; FUNCTION: --  To evaluate a given move in the move list.
;               It first makes the move on the board, then if
;               the move is legal, it evaluates it, and then
;               restores the boaard position.
;
; CALLED BY: -- SORT
;
; CALLS: --     MOVE
;               INCHK
;               PINFND
;               POINTS
;               UNMOV
;
; ARGUMENTS: -- None
;**********************************************************
;EVAL:   CALL    MOVE    ; Make move on the board array
EVAL:    CALL MOVE
;        CALL    INCHK   ; Determine if move is legal
         CALL INCHK
;        ANA     A       ; Legal move ?
         AND al,al
;        JRZ     EV5     ; Yes - jump
         JZ EV5
;        XRA     A       ; Score of zero
         XOR al,al
;        STA     VALM    ; For illegal move
         MOV byte ptr [VALM],al
;        JMP     EV10    ; Jump
         JMP EV10
;EV5:    CALL    PINFND  ; Compile pinned list
EV5:     CALL PINFND
;        CALL    POINTS  ; Assign points to move
         CALL POINTS
;EV10:   CALL    UNMOVE  ; Restore board array
EV10:    CALL UNMOVE
;        RET             ; Return
         RET

;X p60
;**********************************************************
; FIND MOVE ROUTINE
;**********************************************************
; FUNCTION: --  To determine the computer's best move by
;               performing a depth first tree search using
;               the techniques of alpha-beta pruning.
;
; CALLED BY: -- CPTRMV
;
; CALLS: --     PINFND
;               POINTS
;               GENMOV
;               SORTM
;               ASCEND
;               UNMOV
;
; ARGUMENTS: -- None
;**********************************************************
;FNDMOV: LDA     MOVENO  ; Currnet move number
FNDMOV:  MOV AL,byte ptr [MOVENO]
;        CPI     1       ; First move ?
         CMP al,1
;        CZ      BOOK    ; Yes - execute book opening
         JNZ skip24
         CALL BOOK
skip24:
;        XRA     A       ; Initialize ply number to zer
         XOR al,al
;        STA     NPLY
         MOV byte ptr [NPLY],al
;        LXI     H,0     ; Initialize best move to zero
         MOV ebx,0
;        SHLD    BESTM
         MOV [BESTM],ebx
;        LXI     H,MLIST ; Initialize ply list pointers
         MOV ebx,offset MLIST
;        SHLD    MLNXT
         MOV [MLNXT],ebx
;        LXI     H,PLYIX-2
         MOV ebx,offset PLYIX-2
;        SHLD    MLPTRI
         MOV [MLPTRI],ebx
;        LDA     KOLOR   ; Initialize color
         MOV AL,byte ptr [KOLOR]
;        STA     COLOR
         MOV byte ptr [COLOR],al
;        LXI     H,SCORE ; Initialize score index
         MOV ebx,offset SCORE
;        SHLD    SCRIX
         MOV [SCRIX],ebx
;        LDA     PLYMAX  ; Get max ply number
         MOV AL,byte ptr [PLYMAX]
;        ADI     2       ; Add 2
         ADD al,2
;        MOV     B,A     ; Save as counter
         MOV ch,al
;        XRA     A       ; Zero out score table
         XOR al,al
;back05: MOV     M,A
back05:  MOV byte ptr [ebx],al
;        INX     H
         LAHF
         INC ebx
         SAHF
;        DJNZ    back05
         LAHF
         DEC ch
         JNZ back05
         SAHF
;        STA     BC0     ; Zero ply 0 board control
         MOV byte ptr [BC0],al
;        STA     MV0     ; Zero ply 0 material
         MOV byte ptr [MV0],al
;        CALL    PINFND  ; Complie pin list
         CALL PINFND
;        CALL    POINTS  ; Evaluate board at ply 0
         CALL POINTS
;        LDA     BRDC    ; Get board control points
         MOV AL,byte ptr [BRDC]
;        STA     BC0     ; Save
         MOV byte ptr [BC0],al
;        LDA     MTRL    ; Get material count
         MOV AL,byte ptr [MTRL]
;        STA     MV0     ; Save
         MOV byte ptr [MV0],al
;FM5:    LXI     H,NPLY  ; Address of ply counter
FM5:     MOV ebx,offset NPLY
;        INR     M       ; Increment ply count
         INC byte ptr [ebx]
;X p61
;        XRA     A       ; Initialize mate flag
         XOR al,al
;        STA     MATEF
         MOV byte ptr [MATEF],al
;        CALL    GENMOV  ; Generate list of moves
         CALL GENMOV
;        LDA     NPLY    ; Current ply counter
         MOV AL,byte ptr [NPLY]
;        LXI     H,PLYMAX        ; Address of maximum ply number
         MOV ebx,offset PLYMAX
;        CMP     M       ; At max ply ?
         CMP al,byte ptr [ebx]
;        CC      SORTM   ; No - call sort
         JNC skip25
         CALL SORTM
skip25:
;        LHLD    MLPTRI  ; Load ply index pointer
         MOV ebx,[MLPTRI]
;        SHLD    MLPTRJ  ; Save as last move pointer
         MOV [MLPTRJ],ebx
;FM15:   LHLD    MLPTRJ  ; Load last move pointer
FM15:    MOV ebx,[MLPTRJ]
;        MOV     E,M     ; Get next move pointer
         MOV dl,byte ptr [ebx]
;        INX     H
         LAHF
         INC ebx
         SAHF
;        MOV     D,M
         MOV dh,byte ptr [ebx]
;        MOV     A,D
         MOV al,dh
;        ANA     A       ; End of move list ?
         AND al,al
;        JRZ     FM25    ; Yes - jump
         JZ FM25
;        SDED    MLPTRJ  ; Save current move pointer
         MOV [MLPTRJ],edx
;        LHLD    MLPTRI  ; Save in ply pointer list
         MOV ebx,[MLPTRI]
;        MOV     M,E
         MOV byte ptr [ebx],dl
;        INX     H
         LAHF
         INC ebx
         SAHF
;        MOV     M,D
         MOV byte ptr [ebx],dh
;        LDA     NPLY    ; Current ply counter
         MOV AL,byte ptr [NPLY]
;        LXI     H,PLYMAX        ; Maximum ply number ?
         MOV ebx,offset PLYMAX
;        CMP     M       ; Compare
         CMP al,byte ptr [ebx]
;        JRC     FM18    ; Jump if not max
         JC FM18
;        CALL    MOVE    ; Execute move on board array
         CALL MOVE
;        CALL    INCHK   ; Check for legal move
         CALL INCHK
;        ANA     A       ; Is move legal
         AND al,al
;        JRZ     rel017  ; Yes - jump
         JZ rel017
;        CALL    UNMOVE  ; Restore board position
         CALL UNMOVE
;        JMP     FM15    ; Jump
         JMP FM15
;rel017: LDA     NPLY    ; Get ply counter
rel017:  MOV AL,byte ptr [NPLY]
;        LXI     H,PLYMAX        ; Max ply number
         MOV ebx,offset PLYMAX
;        CMP     M       ; Beyond max ply ?
         CMP al,byte ptr [ebx]
;        JRNZ    FM35    ; Yes - jump
         JNZ FM35
;        LDA     COLOR   ; Get current color
         MOV AL,byte ptr [COLOR]
;        XRI     80H     ; Get opposite color
         XOR al,80H
;        CALL    INCHK1  ; Determine if King is in check
         CALL INCHK1
;        ANA     A       ; In check ?
         AND al,al
;        JRZ     FM35    ; No - jump
         JZ FM35
;        JMP     FM19    ; Jump (One more ply for check)
         JMP FM19
;FM18:   LIXD    MLPTRJ  ; Load move pointer
FM18:    MOV esi,[MLPTRJ]
;        MOV     A,MLVAL(X)      ; Get move score
         MOV al,byte ptr [esi+MLVAL]
;        ANA     A       ; Is it zero (illegal move) ?
         AND al,al
;        JRZ     FM15    ; Yes - jump
         JZ FM15
;        CALL    MOVE    ; Execute move on board array
         CALL MOVE
;FM19:   LXI     H,COLOR ; Toggle color
FM19:    MOV ebx,offset COLOR
;        MVI     A,80H
         MOV al,80H
;        XRA     M
         XOR al,byte ptr [ebx]
;        MOV     M,A     ; Save new color
         MOV byte ptr [ebx],al
;X p62
;        BIT     7,A     ; Is it white ?
         MOV ah,al
         AND ah,80h
;        JRNZ    rel018  ; No - jump
         JNZ rel018
;        LXI     H,MOVENO        ; Increment move number
         MOV ebx,offset MOVENO
;        INR     M
         INC byte ptr [ebx]
;rel018: LHLD    SCRIX   ; Load score table pointer
rel018:  MOV ebx,[SCRIX]
;        MOV     A,M     ; Get score two plys above
         MOV al,byte ptr [ebx]
;        INX     H       ; Increment to current ply
         LAHF
         INC ebx
         SAHF
;        INX     H
         LAHF
         INC ebx
         SAHF
;        MOV     M,A     ; Save score as initial value
         MOV byte ptr [ebx],al
;        DCX     H       ; Decrement pointer
         LAHF
         DEC ebx
         SAHF
;        SHLD    SCRIX   ; Save it
         MOV [SCRIX],ebx
;        JMP     FM5     ; Jump
         JMP FM5
;FM25:   LDA     MATEF   ; Get mate flag
FM25:    MOV AL,byte ptr [MATEF]
;        ANA     A       ; Checkmate or stalemate ?
         AND al,al
;        JRNZ    FM30    ; No - jump
         JNZ FM30
;        LDA     CKFLG   ; Get check flag
         MOV AL,byte ptr [CKFLG]
;        ANA     A       ; Was King in check ?
         AND al,al
;        MVI     A,80H   ; Pre-set stalemate score
         MOV al,80H
;        JRZ     FM36    ; No - jump (stalemate)
         JZ FM36
;        LDA     MOVENO  ; Get move number
         MOV AL,byte ptr [MOVENO]
;        STA     PMATE   ; Save
         MOV byte ptr [PMATE],al
;        MVI     A,0FFH  ; Pre-set checkmate score
         MOV al,0FFH
;        JMP     FM36    ; Jump
         JMP FM36
;FM30:   LDA     NPLY    ; Get ply counter
FM30:    MOV AL,byte ptr [NPLY]
;        CPI     1       ; At top of tree ?
         CMP al,1
;        RZ              ; Yes - return
         JNZ skip26
         RET
skip26:
;        CALL    ASCEND  ; Ascend one ply in tree
         CALL ASCEND
;        LHLD    SCRIX   ; Load score table pointer
         MOV ebx,[SCRIX]
;        INX     H       ; Increment to current ply
         LAHF
         INC ebx
         SAHF
;        INX     H
         LAHF
         INC ebx
         SAHF
;        MOV     A,M     ; Get score
         MOV al,byte ptr [ebx]
;        DCX     H       ; Restore pointer
         LAHF
         DEC ebx
         SAHF
;        DCX     H
         LAHF
         DEC ebx
         SAHF
;        JMP     FM37    ; Jump
         JMP FM37
;FM35:   CALL    PINFND  ; Compile pin list
FM35:    CALL PINFND
;        CALL    POINTS  ; Evaluate move
         CALL POINTS
;        CALL    UNMOVE  ; Restore board position
         CALL UNMOVE
;        LDA     VALM    ; Get value of move
         MOV AL,byte ptr [VALM]
;FM36:   LXI     H,MATEF ; Set mate flag
FM36:    MOV ebx,offset MATEF
;        SET     0,M
         LAHF
         OR byte ptr [ebx],1
         SAHF
;        LHLD    SCRIX   ; Load score table pointer
         MOV ebx,[SCRIX]
;FM37:   CMP     M       ; Compare to score 2 ply above
FM37:    CMP al,byte ptr [ebx]
;        JRC     FM40    ; Jump if less
         JC FM40
;        JRZ     FM40    ; Jump if equal
         JZ FM40
;        NEG             ; Negate score
         NEG al
;        INX     H       ; Incr score table pointer
         LAHF
         INC ebx
         SAHF
;        CMP     M       ; Compare to score 1 ply above
         CMP al,byte ptr [ebx]
;        JC      FM15    ; Jump if less than
         JC FM15
;        JZ      FM15    ; Jump if equal
         JZ FM15
;X p63
;        MOV     M,A     ; Save as new score 1 ply above
         MOV byte ptr [ebx],al
;        LDA     NPLY    ; Get current ply counter
         MOV AL,byte ptr [NPLY]
;        CPI     1       ; At top of tree ?
         CMP al,1
;        JNZ     FM15    ; No - jump
         JNZ FM15
;        LHLD    MLPTRJ  ; Load current move pointer
         MOV ebx,[MLPTRJ]
;        SHLD    BESTM   ; Save as best move.pointer
         MOV [BESTM],ebx
;        LDA     SCORE+1 ; Get best move score
         MOV AL,byte ptr [SCORE+1]
;        CPI     0FFH    ; Was it a checkmate ?
         CMP al,0FFH
;        JNZ     FM15    ; No - jump
         JNZ FM15
;        LXI     H,PLYMAX        ; Get maximum ply number
         MOV ebx,offset PLYMAX
;        DCR     M       ; Subtract 2
         DEC byte ptr [ebx]
;        DCR     M
         DEC byte ptr [ebx]
;        LDA     KOLOR   ; Get computer's color
         MOV AL,byte ptr [KOLOR]
;        BIT     7,A     ; Is it white ?
         MOV ah,al
         AND ah,80h
;        RZ              ; Yes - return
         JNZ skip27
         RET
skip27:
;        LXI     H,PMATE ; Checkmate move number
         MOV ebx,offset PMATE
;        DCR     M       ; Decrement
         DEC byte ptr [ebx]
;        RET             ; Return
         RET
;FM40:   CALL    ASCEND  ; Ascend one ply in tree
FM40:    CALL ASCEND
;        JMP     FM15    ; Jump
         JMP FM15

;X p64
;**********************************************************
; ASCEND TREE ROUTINE
;**********************************************************
; FUNCTION: --  To adjust all necessary parameters to
;               ascend one ply in the tree.
;
; CALLED BY: -- FNDMOV
;
; CALLS: --     UNMOV
;
; ARGUMENTS: -- None
;**********************************************************
;ASCEND: LXI     H,COLOR ; Toggle color
ASCEND:  MOV ebx,offset COLOR
;        MVI     A,80H
         MOV al,80H
;        XRA     M
         XOR al,byte ptr [ebx]
;        MOV     M,A     ; Save new color
         MOV byte ptr [ebx],al
;        BIT     7,A     ; Is it white ?
         MOV ah,al
         AND ah,80h
;        JRZ     rel019  ; Yes - jump
         JZ rel019
;        LXI     H,MOVENO        ; Decrement move number
         MOV ebx,offset MOVENO
;        DCR     M
         DEC byte ptr [ebx]
;rel019: LHLD    SCRIX   ; Load score table index
rel019:  MOV ebx,[SCRIX]
;        DCX     H       ; Decrement
         LAHF
         DEC ebx
         SAHF
;        SHLD    SCRIX   ; Save
         MOV [SCRIX],ebx
;        LXI     H,NPLY  ; Decrement ply counter
         MOV ebx,offset NPLY
;        DCR     M
         DEC byte ptr [ebx]
;        LHLD    MLPTRI  ; Load ply list pointer
         MOV ebx,[MLPTRI]
;        DCX     H       ; Load pointer to move list to
         LAHF
         DEC ebx
         SAHF
;        MOV     D,M
         MOV dh,byte ptr [ebx]
;        DCX     H
         LAHF
         DEC ebx
         SAHF
;        MOV     E,M
         MOV dl,byte ptr [ebx]
;        SDED    MLNXT   ; Update move list avail ptr
         MOV [MLNXT],edx
;        DCX     H       ; Get ptr to next move to undo
         LAHF
         DEC ebx
         SAHF
;        MOV     D,M
         MOV dh,byte ptr [ebx]
;        DCX     H
         LAHF
         DEC ebx
         SAHF
;        MOV     E,M
         MOV dl,byte ptr [ebx]
;        SHLD    MLPTRI  ; Save new ply list pointer
         MOV [MLPTRI],ebx
;        SDED    MLPTRJ  ; Save next move pointer
         MOV [MLPTRJ],edx
;        CALL    UNMOVE  ; Restore board to previous pl
         CALL UNMOVE
;        RET             ; Return
         RET

;X p65
;**********************************************************
; ONE MOVE BOOK OPENING
; *****************************************x***************
; FUNCTION: --  To provide an opening book of a single
;               move.
;
; CALLED BY: -- FNDMOV
;
; CALLS: --     None
;
; ARGUMENTS: -- None
;**********************************************************
;BOOK:   POP     PSW             ; Abort return to FNDMOV
BOOK:    POP eax
         sahf
;        LXI     H,SCORE+1       ; Zero out score
         MOV ebx,offset SCORE+1
;        MVI     M,0             ; Zero out score table
         MOV byte ptr [ebx],0
;        LXI     H,BMOVES-2      ; Init best move ptr to book
         MOV ebx,offset BMOVES-2
;        SHLD    BESTM
         MOV [BESTM],ebx
;        LXI     H,BESTM         ; Initialize address of pointer
         MOV ebx,offset BESTM
;        LDA     KOLOR           ; Get computer's color
         MOV AL,byte ptr [KOLOR]
;        ANA     A               ; Is it white ?
         AND al,al
;        JRNZ    BM5             ; No - jump
         JNZ BM5
;        LDAR                    ; Load refresh reg (random no)
         CALL Z80_LDAR
;        BIT     0,A             ; Test random bit
         MOV ah,al
         AND ah,1
;        RZ                      ; Return if zero (P-K4)
         JNZ skip28
         RET
skip28:
;        INR     M               ; P-Q4
         INC byte ptr [ebx]
;        INR     M
         INC byte ptr [ebx]
;        INR     M
         INC byte ptr [ebx]
;        RET                     ; Return
         RET
;BM5:    INR     M               ; Increment to black moves
BM5:     INC byte ptr [ebx]
;        INR     M
         INC byte ptr [ebx]
;        INR     M
         INC byte ptr [ebx]
;        INR     M
         INC byte ptr [ebx]
;        INR     M
         INC byte ptr [ebx]
;        INR     M
         INC byte ptr [ebx]
;        LIXD    MLPTRJ          ; Pointer to opponents 1st move
         MOV esi,[MLPTRJ]
;        MOV     A,MLFRP(X)      ; Get "from" position
         MOV al,byte ptr [esi+MLFRP]
;        CPI     22              ; Is it a Queen Knight move ?
         CMP al,22
;        JRZ     BM9             ; Yes - Jump
         JZ BM9
;        CPI     27              ; Is it a King Knight move ?
         CMP al,27
;        JRZ     BM9             ; Yes - jump
         JZ BM9
;        CPI     34              ; Is it a Queen Pawn ?
         CMP al,34
;        JRZ     BM9             ; Yes - jump
         JZ BM9
;        RC                      ; If Queen side Pawn opening -
         JNC skip29
         RET
skip29:
                                ; return (P-K4)
;        CPI     35              ; Is it a King Pawn ?
         CMP al,35
;        RZ                      ; Yes - return (P-K4)
         JNZ skip30
         RET
skip30:
;BM9:    INR     M               ; (P-Q4)
BM9:     INC byte ptr [ebx]
;        INR     M
         INC byte ptr [ebx]
;        INR     M
         INC byte ptr [ebx]
;        RET                     ; Return to CPTRMV
         RET

;X p73
;**********************************************************
; COMPUTER MOVE ROUTINE
;**********************************************************
; FUNCTION: --  To control the search for the computers move
;               and the display of that move on the board
;               and in the move list.
;
; CALLED BY: -- DRIVER
;
; CALLS: --     FNDMOV
;               FCDMAT
;               MOVE
;               EXECMV
;               BITASN
;               INCHK
;
; MACRO CALLS:  PRTBLK
;               CARRET
;
; ARGUMENTS: -- None
;**********************************************************
;CPTRMV: CALL    FNDMOV          ; Select best move
CPTRMV:  CALL FNDMOV
;        LHLD    BESTM           ; Move list pointer variable
         MOV ebx,[BESTM]
;        SHLD    MLPTRJ          ; Pointer to move data
         MOV [MLPTRJ],ebx
;        LDA     SCORE+1         ; To check for mates
         MOV AL,byte ptr [SCORE+1]
;        CPI     1               ; Mate against computer ?
         CMP al,1
;        JRNZ    CP0C            ; No - jump
         JNZ CP0C
;        MVI     C,1             ; Computer mate flag
         MOV cl,1
;        CALL    FCDMAT          ; Full checkmate ?
         CALL FCDMAT
;CP0C:   CALL    MOVE            ; Produce move on board array
CP0C:    CALL MOVE
;        CALL    EXECMV          ; Make move on graphics board
         CALL EXECMV
                                ; and return info about it
;        MOV     A,B             ; Special move flags
         MOV al,ch
;        ANA     A               ; Special ?
         AND al,al
;        JRNZ    CP10            ; Yes - jump
         JNZ CP10
;        MOV     D,E             ; "To" position of the move
         MOV dh,dl
;        CALL    BITASN          ; Convert to Ascii
         CALL BITASN
;        SHLD    MVEMSG_2    ;todo MVEMSG+3        ; Put in move message
         MOV [MVEMSG_2],ebx
;        MOV     D,C             ; "From" position of the move
         MOV dh,cl
;        CALL    BITASN          ; Convert to Ascii
         CALL BITASN
;        SHLD    MVEMSG          ; Put in move message
         MOV [MVEMSG],ebx
;        PRTBLK  MVEMSG,5        ; Output text of move
         PRTBLK MVEMSG, 5
;        JMPR    CP1C            ; Jump
         JMP CP1C
;CP10:   BIT     1,B             ; King side castle ?
CP10:    MOV ah,ch
         AND ah,2
;        JRZ     rel020          ; No - jump
         JZ rel020
;        PRTBLK  O.O,5           ; Output "O-O"
         PRTBLK O.O, 5
;        JMPR    CP1C            ; Jump
         JMP CP1C
;rel020: BIT     2,B             ; Queen side castle ?
rel020:  MOV ah,ch
         AND ah,4
;        JRZ     rel021          ; No - jump
         JZ rel021
;X p74
;        PRTBLK  O.O.O,5         ;  Output "O-O-O"
         PRTBLK O.O.O, 5
;        JMPR    CP1C            ; Jump
         JMP CP1C
;rel021: PRTBLK  P.PEP,5         ; Output "PxPep" - En passant
rel021:  PRTBLK P.PEP, 5
;CP1C:   LDA     COLOR           ; Should computer call check ?
CP1C:    MOV AL,byte ptr [COLOR]
;        MOV     B,A
         MOV ch,al
;        XRI     80H             ; Toggle color
         XOR al,80H
;        STA     COLOR
         MOV byte ptr [COLOR],al
;        CALL    INCHK           ; Check for check
         CALL INCHK
;        ANA     A               ; Is enemy in check ?
         AND al,al
;        MOV     A,B             ; Restore color
         MOV al,ch
;        STA     COLOR
         MOV byte ptr [COLOR],al
;        JRZ     CP24            ; No - return
         JZ CP24
;        CARRET                  ; New line
         CARRET
;        LDA     SCORE+1         ; Check for player mated
         MOV AL,byte ptr [SCORE+1]
;        CPI     0FFH            ; Forced mate ?
         CMP al,0FFH
;        CNZ     TBCPMV          ; No - Tab to computer column
         JZ skip31
         CALL TBCPMV
skip31:
;        PRTBLK  CKMSG,5         ; Output "check"
         PRTBLK CKMSG, 5
;        LXI     H,LINECT        ; Address of screen line count
         MOV ebx,offset LINECT
;        INR     M               ; Increment for message
         INC byte ptr [ebx]
;CP24:   LDA     SCORE+1         ; Check again for mates
CP24:    MOV AL,byte ptr [SCORE+1]
;        CPI     0FFH            ; Player mated ?
         CMP al,0FFH
;        RNZ                     ; No - return
         JZ skip32
         RET
skip32:
;        MVI     C,0             ; Set player mate flag
         MOV cl,0
;        CALL    FCDMAT          ; Full checkmate ?
         CALL FCDMAT
;        RET                     ; Return
         RET


;X p78
;*****************************************************************
; BOARD INDEX TO ASCII SQUARE NAME
;*****************************************************************
; FUNCTION: --  To translate a hexadecimal index in the
;               board array into an ascii description
;               of the square in algebraic chess notation.
;
; CALLED BY: -- CPTRMV
;
; CALLS: --     DIVIDE
;
; ARGUMENTS: -- Board index input in register D and the Ascii
;               square name is output in register pair HL.
;*****************************************************************
;BITASN: SUB     A               ; Get ready for division
BITASN:  SUB al,al
;        MVI     E,10
         MOV dl,10
;        CALL    DIVIDE          ; Divide
         CALL DIVIDE
;        DCR     D               ; Get rank on 1-8 basis
         DEC dh
;        ADI     60H             ; Convert file to Ascii (a-h)
         ADD al,60H
;        MOV     L,A             ; Save
         MOV bl,al
;        MOV     A,D             ; Rank
         MOV al,dh
;        ADI     30H             ; Convert rank to Ascii (1-8)
         ADD al,30H
;        MOV     H,A             ; Save
         MOV bh,al
;        RET                     ; Return
         RET

;X p79
;*****************************************************************
; PLAYERS MOVE ANALYSIS
;*****************************************************************
; FUNCTION: --  To accept and validate the players move
;               and produce it on the graphics board. Also
;               allows player to resign the game by
;               entering a control-R.
;
; CALLED BY: -- DRIVER
;
; CALLS: --     CHARTR
;               ASNTBI
;               VALMOV
;               EXECMV
;               PGIFND
;               TBPLCL
;
; ARGUMENTS: -- None
;*****************************************************************
;X p80
;**********************************************************
; ASCII SQUARE NAME TO BOARD INDEX
;**********************************************************
; FUNCTION: --  To convert an algebraic square name in
;               Ascii to a hexadecimal board index.
;               This routine also checks the input for
;               validity.
;
; CALLED BY: -- PLYRMV
;
; CALLS: --     MLTPLY
;
; ARGUMENTS: -- Accepts the square name in register pair HL and
;               outputs the board index in register A. Register
;               B = 0 if ok. Register B = Register A if invalid.
;**********************************************************
;ASNTBI: MOV     A,L             ; Ascii rank (1 - 8)
ASNTBI:  MOV al,bl
;        SUI     30H             ; Rank 1 - 8
         SUB al,30H
;        CPI     1               ; Check lower bound
         CMP al,1
;        JM      AT04            ; Jump if invalid
         JS AT04
;        CPI     9               ; Check upper bound
         CMP al,9
;        JRNC    AT04            ; Jump if invalid
         JNC AT04
;        INR     A               ; Rank 2 - 9
         INC al
;        MOV     D,A             ; Ready for multiplication
         MOV dh,al
;        MVI     E,10
         MOV dl,10
;        CALL    MLTPLY          ; Multiply
         CALL MLTPLY
;        MOV     A,H             ; Ascii file letter (a - h)
         MOV al,bh
;        SUI     40H             ; File 1 - 8
         SUB al,40H
;        CPI     1               ; Check lower bound
         CMP al,1
;        JM      AT04            ; Jump if invalid
         JS AT04
;        CPI     9               ; Check upper bound
         CMP al,9
;        JRNC    AT04            ; Jump if invalid
         JNC AT04
;        ADD     D               ; File+Rank(20-90)=Board index
         ADD al,dh
;        MVI     B,0             ; Ok flag
         MOV ch,0
;        RET                     ; Return
         RET
;AT04:   MOV     B,A             ; Invalid flag
AT04:    MOV ch,al
;        RET                     ; Return
         RET

;X p81
;*************************************************************
; VALIDATE MOVE SUBROUTINE
;*************************************************************
; FUNCTION: --  To check a players move for validity.
;
; CALLED BY: -- PLYRMV
;
; CALLS: --     GENMOV
;               MOVE
;               INCHK
;               UNMOVE
;
; ARGUMENTS: -- Returns flag in register A, 0 for valid and 1 for
;               invalid move.
;*************************************************************
;VALMOV: LHLD    MLPTRJ          ; Save last move pointer
VALMOV:  MOV ebx,[MLPTRJ]
;        PUSH    H               ; Save register
         PUSH ebx
;        LDA     KOLOR           ; Computers color
         MOV AL,byte ptr [KOLOR]
;        XRI     80H             ; Toggle color
         XOR al,80H
;        STA     COLOR           ; Store
         MOV byte ptr [COLOR],al
;        LXI     H,PLYIX-2       ; Load move list index
         MOV ebx,offset PLYIX-2
;        SHLD    MLPTRI
         MOV [MLPTRI],ebx
;        LXI     H,MLIST+1024    ; Next available list pointer
         MOV ebx,offset MLIST+1024
;        SHLD    MLNXT
         MOV [MLNXT],ebx
;        CALL    GENMOV          ; Generate opponents moves
         CALL GENMOV
;        LXI     X,MLIST+1024    ; Index to start of moves
         MOV esi,offset MLIST+1024
;VA5:    LDA     MVEMSG          ; "From" position
VA5:     MOV AL,byte ptr [MVEMSG]
;        CMP     MLFRP(X)        ; Is it in list ?
         CMP al,byte ptr [esi+MLFRP]
;        JRNZ    VA6             ; No - jump
         JNZ VA6
;        LDA     MVEMSG+1        ; "To" position
         MOV AL,byte ptr [MVEMSG+1]
;        CMP     MLTOP(X)        ; Is it in list ?
         CMP al,byte ptr [esi+MLTOP]
;        JRZ     VA7             ; Yes - jump
         JZ VA7
;VA6:    MOV     E,MLPTR(X)      ; Pointer to next list move
VA6:     MOV dl,byte ptr [esi+MLPTR]
;        MOV     D,MLPTR+1(X)
         MOV dh,byte ptr [esi+MLPTR+1]
;        XRA     A               ; At end of list ?
         XOR al,al
;        CMP     D
         CMP al,dh
;        JRZ     VA10            ; Yes - jump
         JZ VA10
;        PUSH    D               ; Move to X register
         PUSH edx
;        POP     X
         POP esi
;        JMPR    VA5             ; Jump
         JMP VA5
;VA7:    SIXD    MLPTRJ          ; Save opponents move pointer
VA7:     MOV [MLPTRJ],esi
;        CALL    MOVE            ; Make move on board array
         CALL MOVE
;        CALL    INCHK           ; Was it a legal move ?
         CALL INCHK
;        ANA     A
         AND al,al
;        JRNZ    VA9             ; No - jump
         JNZ VA9
;VA8:    POP     H               ; Restore saved register
VA8:     POP ebx
;        RET                     ; Return
         RET
;VA9:    CALL    UNMOVE          ; Un-do move on board array
VA9:     CALL UNMOVE
;VA10:   MVI     A,1             ; Set flag for invalid move
VA10:    MOV al,1
;        POP     H               ; Restore saved register
         POP ebx
;        SHLD    MLPTRJ          ; Save move pointer
         MOV [MLPTRJ],ebx
;        RET                     ; Return
         RET

        
;X p88
;*************************************************************
; UPDATE POSITIONS OF ROYALTY
;*************************************************************
; FUNCTION: --  To update the positions of the Kings
;               and Queen after a change of board position
;               in ANALYS.
;
; CALLED BY: -- ANALYS
;
; CALLS: --     None
;
; ARGUMENTS: -- None
;*************************************************************
;ROYALT: LXI     H,POSK          ; Start of Royalty array
ROYALT:  MOV ebx,offset POSK
;        MVI     B,4             ; Clear all four positions
         MOV ch,4
;back06: MVI     M,0
back06:  MOV byte ptr [ebx],0
;        INX     H
         LAHF
         INC ebx
         SAHF
;        DJNZ    back06
         LAHF
         DEC ch
         JNZ back06
         SAHF
;        MVI     A,21            ; First board position
         MOV al,21
;RY04:   STA     M1              ; Set up board index
RY04:    MOV byte ptr [M1],al
;        LXI     H,POSK          ; Address of King position
         MOV ebx,offset POSK
;        LIXD    M1
         MOV esi,[M1]
;        MOV     A,BOARD(X)      ; Fetch board contents
         MOV al,byte ptr [esi+BOARD]
;        BIT     7,A             ; Test color bit
         MOV ah,al
         AND ah,80h
;        JRZ     rel023          ; Jump if white
         JZ rel023
;        INX     H               ; Offset for black
         LAHF
         INC ebx
         SAHF
;rel023: ANI     7               ; Delete flags, leave piece
rel023:  AND al,7
;        CPI     KING            ; King ?
         CMP al,KING
;        JRZ     RY08            ; Yes - jump
         JZ RY08
;        CPI     QUEEN           ; Queen ?
         CMP al,QUEEN
;        JRNZ    RY0C            ; No - jump
         JNZ RY0C
;        INX     H               ; Queen position
         LAHF
         INC ebx
         SAHF
;        INX     H               ; Plus offset
         LAHF
         INC ebx
         SAHF
;RY08:   LDA     M1              ; Index
RY08:    MOV AL,byte ptr [M1]
;        MOV     M,A             ; Save
         MOV byte ptr [ebx],al
;RY0C:   LDA     M1              ; Current position
RY0C:    MOV AL,byte ptr [M1]
;        INR     A               ; Next position
         INC al
;        CPI     99              ; Done.?
         CMP al,99
;        JRNZ    RY04            ; No - jump
         JNZ RY04
;        RET                     ; Return
         RET


;X p93
;**********************************************************
; BOARD INDEX TO NORM ADDRESS SUBR.
;**********************************************************
; FUNCTION: --  Converts a hexadecimal board index into
;               a Norm address for the square.
;
; CALLED BY: -- DSPBRD
;               INSPCE
;               ANALYS
;               MATED
;
; CALLS: --     DIVIDE
;               MLTPLY
;
;ARGUMENTS: --  Returns the Norm address in register pair
;               HL.
;**********************************************************
;CONVRT: PUSH    B       ; Save registers
CONVRT:  PUSH ecx
;        PUSH    D
         PUSH edx
;        PUSH    PSW
         lahf
         PUSH eax
;        LDA     BRDPOS  ; Get board index
         MOV AL,byte ptr [BRDPOS]
;        MOV     D,A     ; Set up dividend
         MOV dh,al
;        SUB     A
         SUB al,al
;        MVI     E,10    ; Divisor
         MOV dl,10
;        CALL    DIVIDE  ; Index into rank and file
         CALL DIVIDE
                        ; file (1-8) & rank (2-9)
;        DCR     D       ; For rank (1-8)
         DEC dh
;        DCR     A       ; For file (0-7)
         DEC al
;        MOV     C,D     ; Save
         MOV cl,dh
;        MVI     D,6     ; Multiplier
         MOV dh,6
;        MOV     E,A     ; File number is multiplicand
         MOV dl,al
;        CALL    MLTPLY  ; Giving file displacement
         CALL MLTPLY
;        MOV     A,D     ; Save
         MOV al,dh
;        ADI     10H     ; File norm address
         ADD al,10H
;        MOV     L,A     ; Low order address byte
         MOV bl,al
;        MVI     A,8     ; Rank adjust
         MOV al,8
;        SUB     C       ; Rank displacement
         SUB al,cl
;        ADI     0C0H    ; Rank Norm address
         ADD al,0C0H
;        MOV     H,A     ; High order addres byte
         MOV bh,al
;        POP     PSW     ; Restore registers
         POP eax
         sahf
;        POP     D
         POP edx
;        POP     B
         POP ecx
;        RET             ; Return
         RET

;X p94
;**********************************************************
; POSITIVE INTEGER DIVISION
;**********************************************************
;DIVIDE: PUSH    B
DIVIDE:  PUSH ecx
;        MVI     B,8
         MOV ch,8
;DD04:   SLAR    D
DD04:    SHL dh,1
;        RAL
         RCL al,1
;        SUB     E
         SUB al,dl
;        JM      rel024
         JS rel024
;        INR     D
         INC dh
;        JMPR    rel024
         JMP rel024
;        ADD     E
         ADD al,dl
;rel024: DJNZ    DD04
rel024:  LAHF
         DEC ch
         JNZ DD04
         SAHF
;        POP     B
         POP ecx
;        RET
         RET

;**********************************************************
; POSITIVE INTEGER MULTIPLICATION
;**********************************************************
;MLTPLY: PUSH    B
MLTPLY:  PUSH ecx
;        SUB     A
         SUB al,al
;        MVI     B,8
         MOV ch,8
;ML04:   BIT     0,D
ML04:    MOV ah,dh
         AND ah,1
;        JRZ     rel025
         JZ rel025
;        ADD     E
         ADD al,dl
;rel025: SRAR    A
rel025:  SAR al,1
;        RARR    D
         RCR dh, 1
;        DJNZ    ML04
         LAHF
         DEC ch
         JNZ ML04
         SAHF
;        POP     B
         POP ecx
;        RET
         RET

;X p95

;**********************************************************
; EXECUTE MOVE SUBROUTINE
;**********************************************************
; FUNCTION: --  This routine is the control routine for
;               MAKEMV. It checks for double moves and
;               sees that they are properly handled. It
;               sets flags in the B register for double
;               moves:
;                       En Passant -- Bit 0
;                       O-O     -- Bit 1
;                       O-O-O   -- Bit 2
;
; CALLED BY: -- PLYRMV
;               CPTRMV
;
; CALLS: --     MAKEMV
;
; ARGUMENTS: -- Flags set in the B register as described
;               above.
;**********************************************************
;EXECMV: PUSH    X               ; Save registers
EXECMV:  PUSH esi
;        PUSH    PSW
         lahf
         PUSH eax
;        LIXD    MLPTRJ          ; Index into move list
         MOV esi,[MLPTRJ]
;        MOV     C,MLFRP(X)      ; Move list "from" position
         MOV cl,byte ptr [esi+MLFRP]
;        MOV     E,MLTOP(X)      ; Move list "to" position
         MOV dl,byte ptr [esi+MLTOP]
;        CALL    MAKEMV          ; Produce move
         CALL MAKEMV
;        MOV     D,MLFLG(X)      ; Move list flags
         MOV dh,byte ptr [esi+MLFLG]
;        MVI     B,0
         MOV ch,0
;        BIT     6,D             ; Double move ?
         MOV ah,dh
         AND ah,40h
;        JRZ     EX14            ; No - jump
         JZ EX14
;        LXI     D,8             ; Move list entry width
         MOV edx,8
;        DADX    D               ; Increment MLPTRJ
         LAHF
         ADD esi,edx
         SAHF
;        MOV     C,MLFRP(X)      ; Second "from" position
         MOV cl,byte ptr [esi+MLFRP]
;        MOV     E,MLTOP(X)      ; Second "to" position
         MOV dl,byte ptr [esi+MLTOP]
;        MOV     A,E             ; Get "to" position
         MOV al,dl
;        CMP     C               ; Same as "from" position ?
         CMP al,cl
;        JRNZ    EX04            ; No - jump
         JNZ EX04
;        INR     B               ; Set en passant flag
         INC ch
;        JMPR    EX10            ; Jump
         JMP EX10
;EX04:   CPI     1AH             ; White O-O ?
EX04:    CMP al,1AH
;        JRNZ    EX08            ; No - jump
         JNZ EX08
;        SET     1,B             ; Set O-O flag
         LAHF
         OR ch,2
         SAHF
;        JMPR    EX10            ; Jump
         JMP EX10
;EX08:   CPI     60H             ; Black 0-0 ?
EX08:    CMP al,60H
;        JRNZ    EX0C            ; No - jump
         JNZ EX0C
;        SET     1,B             ; Set 0-0 flag
         LAHF
         OR ch,2
         SAHF
;        JMPR    EX10            ; Jump
         JMP EX10
;EX0C:   SET     2,B             ; Set 0-0-0 flag
EX0C:    LAHF
         OR ch,4
         SAHF
;EX10:   CALL    MAKEMV          ; Make 2nd move on board
EX10:    CALL MAKEMV
;EX14:   POP     PSW             ; Restore registers
EX14:    POP eax
         sahf
;        POP     X
         POP esi
;        RET                     ; Return
         RET

;X p98
;**********************************************************
; MAKE MOVE SUBROUTINE
;**********************************************************
; FUNDTION: --  Moves the piece on the board when a move
;               is made. It blinks both the "from" and
;               "to" positions to give notice of the move.
;
; CALLED BY: -- EXECMV
;
; CALLS: --     CONVRT
;               BLNKER
;               INSPCE
;
; ARGUMENTS: -- The "from" position is passed in register
;               C, and the "to" position in register E.
;**********************************************************
;MAKEMV: PUSH    PSW     ; Save register
MAKEMV:  lahf
         PUSH eax
;        PUSH    B
         PUSH ecx
;        PUSH    D
         PUSH edx
;        PUSH    H
         PUSH ebx
;        MOV     A,C     ; "From" position
         MOV al,cl
;        STA     BRDPOS  ; Set up parameter
         MOV byte ptr [BRDPOS],al
;        CALL    CONVRT  ; Getting Norm address in HL
         CALL CONVRT
;        MVI     B,10    ; Blink parameter
         MOV ch,10
;        CALL    BLNKER  ; Blink "from" square
         CALL BLNKER
;        MOV     A,M     ; Bring in Norm 1plock
         MOV al,byte ptr [ebx]
;        INR     L       ; First change block
         INC bl
;        MVI     D,0     ; Bar counter
         MOV dh,0
;MM04:   MVI     B,4     ; Block counter
MM04:    MOV ch,4
;MM08:   MOV     M,A     ; Insert blank block
MM08:    MOV byte ptr [ebx],al
;        INR     L       ; Next change block
         INC bl
;        DJNZ    MM08    ; Done ? No - jump
         LAHF
         DEC ch
         JNZ MM08
         SAHF
;        MOV     C,A     ; Saving norm block
         MOV cl,al
;        MOV     A,L     ; Bar increment
         MOV al,bl
;        ADI     3CH
         ADD al,3CH
;        MOV     L,A
         MOV bl,al
;        MOV     A,C     ; Restore Norm block
         MOV al,cl
;        INR     D
         INC dh
;        BIT     2,D     ; Done ?
         MOV ah,dh
         AND ah,4
;        JRZ     MM04    ; No - jump
         JZ MM04
;        MOV     A,E     ; Get "to" position
         MOV al,dl
;        STA     BRDPOS  ; Set up parameter
         MOV byte ptr [BRDPOS],al
;        CALL    CONVRT  ; Getting Norm address in HL
         CALL CONVRT
;        MVI     B,10    ; Blink parameter
         MOV ch,10
;        CALL    INSPCE  ; Inserts the piece
         CALL INSPCE
;        CALL    BLNKER  ; Blinks "to" square
         CALL BLNKER
;        POP     H       ; Restore registers
         POP ebx
;        POP     D
         POP edx
;        POP     B
         POP ecx
;        POP     PSW
         POP eax
         sahf
;        RET             ; Return
         RET
INITBD  ENDP
;
; SHIM from C code
;
PUBLIC	_shim_function
_shim_function PROC
    push    ebp
    mov     ebp,esp
    push    esi
    push    edi
    mov     ebx,[ebp+8]
    mov     dword ptr [ebx],offset BOARDA

;        SUB     A               ; Code of White is zero
    sub al,al
;        STA     COLOR           ; White always moves first
    mov byte ptr [COLOR],al
;        STA     KOLOR           ; Bring in computer's color
    mov byte ptr [KOLOR],al
;        CALL    INTERR          ; Players color/search depth
;   call    INTERR
    mov byte ptr [PLYMAX],1
    mov al,0
;        CALL    INITBD          ; Initialize board array
    call    INITBD
;        MVI     A,1             ; Move number is 1 at at start
    mov al,1
;        STA     MOVENO          ; Save
    mov byte ptr [MOVENO],al
;        CALL    CPTRMV          ; Make and write computers move
    mov al,1
    call    INITBD
    pop     edi
    pop     esi
    pop     ebp
	ret
_shim_function ENDP



;        .END
_TEXT    ENDS
END

;        .END    DRIVER  ;X Define Program Entry Point
;X*********************************************************
;X*********************************************************
;X*********************************************************
;X*********************************************************
;X*********************************************************
;X*********************************************************
;X*********************************************************
;X*********************************************************
