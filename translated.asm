;X p19
;**********************************************************
;               SARGON
;
;       Sargon is a computer chess playing program designed
; and coded by Dan and Kathe Spracklen. Copyright 1978. All
; rights reserved. No part of this publication may be
; reproduced without the prior written permission.
;**********************************************************

         .686P
         .XMM
         .model  flat

CCIR     MACRO                          ;todo
         ENDM
PRTBLK   MACRO   name,len               ;todo
         ENDM
CARRET   MACRO                          ;todo
         ENDM

Z80_EXAF MACRO
         lahf
         xchg    eax,shadow_eax
         sahf
         ENDM

Z80_EXX  MACRO
         xchg    ebx,shadow_ebx
         xchg    ecx,shadow_ecx
         xchg    edx,shadow_edx
         ENDM

Z80_RLD  MACRO                          ;a=kx (hl)=yz -> a=ky (hl)=zx
         mov     ah,byte ptr [ebx]      ;ax=yzkx
         ror     al,4                   ;ax=yzxk
         rol     ax,4                   ;ax=zxky
         mov     byte ptr [ebx],ah      ;al=ky [ebx]=zx
         or      al,al                  ;set z and s flags
         ENDM

Z80_RRD  MACRO                          ;a=kx (hl)=yz -> a=kz (hl)=xy
         mov     ah,byte ptr [ebx]      ;ax=yzkx
         ror     ax,4                   ;ax=xyzk
         ror     al,4                   ;ax=xykz
         mov     byte ptr [ebx],ah      ;al=kz [ebx]=xy
         or      al,al                  ;set z and s flags
         ENDM

Z80_LDAR MACRO                          ;to get random number
         pushf                          ;maybe there's entropy in stack junk
         push    ebx
         mov     ebx,ebp
         mov     ax,0
         xor     al,byte ptr [ebx]
         dec     ebx
         jz      $+6
         dec     ah
         jnz     $-7
         pop     ebx
         popf
         ENDM

        
;**********************************************************
; EQUATES
;**********************************************************
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
_DATA    SEGMENT

;START   DS      100h
START    DB      100h DUP (?)
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
;DIRECT  DB      +09,+11,-11,-09
DIRECT   DB      +09,+11,-11,-09
;        DB      +10,-10,+01,-01
         DB      +10,-10,+01,-01
;        DB      -21,-12,+08,+19
         DB      -21,-12,+08,+19
;        DB      +21,+12,-08,-19
         DB      +21,+12,-08,-19
;        DB      +10,+10,+11,+09
         DB      +10,+10,+11,+09
;        DB      -10,-10,-11,-09
         DB      -10,-10,-11,-09
;X p20
;**********************************************************
; DPOINT   --   Direction Table Pointer. Used to determine
;               where to begin in the direction table for any
;               given piece.
;**********************************************************
;DPOINT =        .-TBASE
;DPOINT = 18h
DPOINT   EQU     18h

;        DB      20,16,8,0,4,0,0
         DB      20,16,8,0,4,0,0

;**********************************************************
; DCOUNT   --   Direction Table Counter. Used to determine
;               the number of directions of movement for any
;               given piece.
;**********************************************************
;DCOUNT =        .-TBASE
;DCOUNT = 1fh
DCOUNT   EQU     1fh
;        DB      4,4,8,4,4,8,8
         DB      4,4,8,4,4,8,8

;**********************************************************
; PVALUE   --   Point Value. Gives the point value of each
;               piece, or the worth of each piece.
;**********************************************************
;PVALUE =        .-TBASE-1
;PVALUE = 25h
PVALUE   EQU     25h
;        DB      1,3,3,5,9,10
         DB      1,3,3,5,9,10

;**********************************************************
; PIECES   --   The initial arrangement of the first rank of
;               pieces on the board. Use to set up the board
;               for the start of the game.
;**********************************************************
;PIECES =        .-TBASE
;PIECES = 2ch
PIECES   EQU     2ch
;        DB      4,2,3,5,6,3,2,4
         DB      4,2,3,5,6,3,2,4
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
;BOARDA  DS      120
BOARDA   DB      120 DUP (?)

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
;ATKLST  DW      0,0,0,0,0,0,0
ATKLST   DD      0,0,0,0,0,0,0

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
;PLISTA  DW      0,0,0,0,0,0,0,0,0,0
PLISTA   DD      0,0,0,0,0,0,0,0,0,0

;**********************************************************
; POSK  --      Position of Kings. A two byte area, the first
;               byte of which hold the position of the white
;               king and the second holding the position of
;               the black king.
;
; POSQ  --      Position of Queens. Like POSK,but for queens.
;**********************************************************
;POSK    DB      24,95
POSK     DB      24,95
;POSQ    DB      14,94
POSQ     DB      14,94
;        DB      -1
         DB      -1

;X p23
;**********************************************************
; SCORE --      Score Array. Used during Alpha-Beta pruning to
;               hold the scores at each ply. It includes two
;               "dummy" entries for ply -1 and ply 0.
;**********************************************************
;SCORE   DW      0,0,0,0,0,0
SCORE    DD      0,0,0,0,0,0

;**********************************************************
; PLYIX --      Ply Table. Contains pairs of pointers, a pair
;               for each ply. The first pointer points to the
;               top of the list of possible moves at that ply.
;               The second pointer points to which move in the
;               list is the one currently being considered.
;**********************************************************
;PLYIX   DW      0,0,0,0,0,0,0,0,0,0
PLYIX    DD      0,0,0,0,0,0,0,0,0,0
;        DW      0,0,0,0,0,0,0,0,0,0
         DD      0,0,0,0,0,0,0,0,0,0

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
;        ORG     START+0
                                        ;ORG     START+0
;M1      DW      TBASE
M1       DD      TBASE
;M2      DW      TBASE
M2       DD      TBASE
;M3      DW      TBASE
M3       DD      TBASE
;M4      DW      TBASE
M4       DD      TBASE
;T1      DW      TBASE
T1       DD      TBASE
;T2      DW      TBASE
T2       DD      TBASE
;T3      DW      TBASE
T3       DD      TBASE
;INDX1   DW      TBASE
INDX1    DD      TBASE
;INDX2   DW      TBASE
INDX2    DD      TBASE
;NPINS   DW      TBASE
NPINS    DD      TBASE
;MLPTRI  DW      PLYIX
MLPTRI   DD      PLYIX
;MLPTRJ  DW      0
MLPTRJ   DD      0
;SCRIX   DW      0
SCRIX    DD      0
;BESTM   DW      0
BESTM    DD      0
;MLLST   DW      0
MLLST    DD      0
;MLNXT   DW      MLIST
MLNXT    DD      MLIST

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
;KOLOR   DB      0
KOLOR    DB      0
;COLOR   DB      0
COLOR    DB      0
;P1      DB      0
P1       DB      0
;P2      DB      0
P2       DB      0
;P3      DB      0
P3       DB      0
;PMATE   DB      0
PMATE    DB      0
;MOVENO  DB      0
MOVENO   DB      0
;PLYMAX  DB      2
PLYMAX   DB      2
;NPLY    DB      0
NPLY     DB      0
;CKFLG   DB      0
CKFLG    DB      0
;MATEF   DB      0
MATEF    DB      0
;VALM    DB      0
VALM     DB      0
;BRDC    DB      0
BRDC     DB      0
;PTSL    DB      0
PTSL     DB      0
;PTSW1   DB      0
PTSW1    DB      0
;PTSW2   DB      0
PTSW2    DB      0
;MTRL    DB      0
MTRL     DB      0
;BC0     DB      0
BC0      DB      0
;MV0     DB      0
MV0      DB      0
;PTSCK   DB      0
PTSCK    DB      0
;BMOVES  DB      35,55,10H
BMOVES   DB      35,55,10H
;        DB      34,54,10H
         DB      34,54,10H
;        DB      85,65,10H
         DB      85,65,10H
;        DB      84,64,10H
         DB      84,64,10H

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
;MVEMSG  DW      0
MVEMSG   DD      0
;MVEMSG_2        DW      0
MVEMSG_2 DD      0
;BRDPOS  DS      1                      ; Index into the board array
BRDPOS   DB      1 DUP (?)
;ANBDPS  DS      1                      ; Additional index required for ANALYS
ANBDPS   DB      1 DUP (?)
;LINECT  DB      0                      ; Current line number
LINECT   DB      0
;**** TEMP TODO END

;X p28
;        ORG     START+3F0H             ;X START+300H
                                        ;ORG     START+3F0H
;MLIST   DS      2048
MLIST    DB      2048 DUP (?)
;MLEND   DW      0
MLEND    DD      0
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
;        DS      8
         DB      100 DUP (?)

shadow_eax  dd   0
shadow_ebx  dd   0
shadow_ecx  dd   0
shadow_edx  dd   0
_DATA    ENDS
_TEXT    SEGMENT

;**********************************************************
; PROGRAM CODE SECTION
;**********************************************************
DISPATCH PROC
         cmp al,0
         jz  INITBD
         cmp al,1
         jz  CPTRMV
         ret
         
;FCDMAT: RET
FCDMAT:  RET
;TBCPMV: RET
TBCPMV:  RET
;INSPCE: RET
INSPCE:  RET
;BLNKER: RET
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
;INITBD: LD      ch,120                 ; Pre-fill board with -1's
INITBD:  MOV     ch,120
;        LD      bx,BOARDA
         MOV     ebx,offset BOARDA
;back01: LD      (bx),-1
back01:  MOV     byte ptr [ebx],-1
;        INC     bx
         LEA     ebx,[ebx+1]
;        DJNZ    back01
         LAHF
         DEC ch
         JNZ     back01
         SAHF
;        LD      ch,8
         MOV     ch,8
;        LD      si,BOARDA
         MOV     esi,offset BOARDA
;IB2:    LD      al,(si-8)              ; Fill non-border squares
IB2:     MOV     al,byte ptr [esi-8]
;        LD      (si+21),al             ; White pieces
         MOV     byte ptr [esi+21],al
;        SET     7,al                   ; Change to black
         LAHF
         OR      al,80h
         SAHF
;        LD      (si+91),al             ; Black pieces
         MOV     byte ptr [esi+91],al
;        LD      (si+31),PAWN           ; White Pawns
         MOV     byte ptr [esi+31],PAWN
;        LD      (si+81),BPAWN          ; Black Pawns
         MOV     byte ptr [esi+81],BPAWN
;        LD      (si+41),0              ; Empty squares
         MOV     byte ptr [esi+41],0
;        LD      (si+51),0
         MOV     byte ptr [esi+51],0
;        LD      (si+61),0
         MOV     byte ptr [esi+61],0
;        LD      (si+71),0
         MOV     byte ptr [esi+71],0
;        INC     si
         LEA     esi,[esi+1]
;        DJNZ    IB2
         LAHF
         DEC ch
         JNZ     IB2
         SAHF
;        LD      si,POSK                ; Init King/Queen position list
         MOV     esi,offset POSK
;        LD      (si+0),25
         MOV     byte ptr [esi+0],25
;        LD      (si+1),95
         MOV     byte ptr [esi+1],95
;        LD      (si+2),24
         MOV     byte ptr [esi+2],24
;        LD      (si+3),94
         MOV     byte ptr [esi+3],94
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
;PATH:   LD      bx,M2                  ; Get previous position
PATH:    MOV     ebx,offset M2
;        LD      al,(bx)
         MOV     al,byte ptr [ebx]
;        ADD     al,cl                  ; Add direction constant
         ADD     al,cl
;        LD      (bx),al                ; Save new position
         MOV     byte ptr [ebx],al
;        LD      si,(M2)                ; Load board index
         MOV     esi,[M2]
;        LD      al,(si+BOARD)          ; Get contents of board
         MOV     al,byte ptr [esi+BOARD]
;        CMP     al,-1                  ; In border area ?
         CMP     al,-1
;        JR      Z,PA2                  ; Yes - jump
         JZ      PA2
;        LD      (P2),al                ; Save piece
         MOV     byte ptr [P2],al
;        AND     al,7                   ; Clear flags
         AND     al,7
;        LD      (T2),al                ; Save piece type
         MOV     byte ptr [T2],al
;        RET     Z                      ; Return if empty
         JNZ     skip1
         RET
skip1:
;        LD      al,(P2)                ; Get piece encountered
         MOV     al,byte ptr [P2]
;        LD      bx,P1                  ; Get moving piece address
         MOV     ebx,offset P1
;        XOR     al,(bx)                ; Compare
         XOR     al,byte ptr [ebx]
;        BIT     7,al                   ; Do colors match ?
         TEST    al,80h
;        JR      Z,PA1                  ; Yes - jump
         JZ      PA1
;        LD      al,1                   ; Set different color flag
         MOV     al,1
;        RET                            ; Return
         RET
;PA1:    LD      al,2                   ; Set same color flag
PA1:     MOV     al,2
;        RET                            ; Return
         RET
;PA2:    LD      al,3                   ; Set off board flag
PA2:     MOV     al,3
;        RET                            ; Return
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
;MPIECE: XOR     al,(bx)                ; Piece to move
MPIECE:  XOR     al,byte ptr [ebx]
;        AND     al,87H                 ; Clear flag bit
         AND     al,87H
;        CMP     al,BPAWN               ; Is it a black Pawn ?
         CMP     al,BPAWN
;        JR      NZ,rel001              ; No-Skip
         JNZ     rel001
;        DEC     al                     ; Decrement for black Pawns
         DEC     al
;rel001: AND     al,7                   ; Get piece type
rel001:  AND     al,7
;        LD      (T1),al                ; Save piece type
         MOV     byte ptr [T1],al
;        LD      di,(T1)                ; Load index to DCOUNT/DPOINT
         MOV     edi,[T1]
;        LD      ch,(di+DCOUNT)         ; Get direction count
         MOV     ch,byte ptr [edi+DCOUNT]
;        LD      al,(di+DPOINT)         ; Get direction pointer
         MOV     al,byte ptr [edi+DPOINT]
;        LD      (INDX2),al             ; Save as index to direct
         MOV     byte ptr [INDX2],al
;        LD      di,(INDX2)             ; Load index
         MOV     edi,[INDX2]
;MP5:    LD      cl,(di+DIRECT)         ; Get move direction
MP5:     MOV     cl,byte ptr [edi+DIRECT]
;        LD      al,(M1)                ; From position
         MOV     al,byte ptr [M1]
;        LD      (M2),al                ; Initialize to position
         MOV     byte ptr [M2],al
;MP10:   CALL    PATH                   ; Calculate next position
MP10:    CALL    PATH
;        CMP     al,2                   ; Ready for new direction ?
         CMP     al,2
;        JR      NC,MP15                ; Yes - Jump
         JNC     MP15
;        AND     al,al                  ; Test for empty square
         AND     al,al
;        EXAF                           ; Save result
         Z80_EXAF
;        LD      al,(T1)                ; Get piece moved
         MOV     al,byte ptr [T1]
;        CMP     al,PAWN+1              ; Is it a Pawn ?
         CMP     al,PAWN+1
;        JR      C,MP20                 ; Yes - Jump
         JC      MP20
;        CALL    ADMOVE                 ; Add move to list
         CALL    ADMOVE
;        EXAF                           ; Empty square ?
         Z80_EXAF
;        JR      NZ,MP15                ; No - Jump
         JNZ     MP15
;        LD      al,(T1)                ; Piece type
         MOV     al,byte ptr [T1]
;        CMP     al,KING                ; King ?
         CMP     al,KING
;        JR      Z,MP15                 ; Yes - Jump
         JZ      MP15
;        CMP     al,BISHOP              ; Bishop, Rook, or Queen ?
         CMP     al,BISHOP
;        JR      NC,MP10                ; Yes - Jump
         JNC     MP10
;MP15:   INC     di                     ; Increment direction index
MP15:    LEA     edi,[edi+1]
;        DJNZ    MP5                    ; Decr. count-jump if non-zerc
         LAHF
         DEC ch
         JNZ     MP5
         SAHF
;        LD      al,(T1)                ; Piece type
         MOV     al,byte ptr [T1]
;X p31
;        CMP     al,KING                ; King ?
         CMP     al,KING
;        CALL    Z,CASTLE               ; Yes - Try Castling
         JNZ     skip2
         CALL    CASTLE
skip2:
;        RET                            ; Return
         RET
; ***** PAWN LOGIC *****
;MP20:   LD      al,ch                  ; Counter for direction
MP20:    MOV     al,ch
;        CMP     al,3                   ; On diagonal moves ?
         CMP     al,3
;        JR      C,MP35                 ; Yes - Jump
         JC      MP35
;        JR      Z,MP30                 ; -or-jump if on 2 square move
         JZ      MP30
;        EXAF                           ; Is forward square empty?
         Z80_EXAF
;        JR      NZ,MP15                ; No - jump
         JNZ     MP15
;        LD      al,(M2)                ; Get "to" position
         MOV     al,byte ptr [M2]
;        CMP     al,91                  ; Promote white Pawn ?
         CMP     al,91
;        JR      NC,MP25                ; Yes - Jump
         JNC     MP25
;        CMP     al,29                  ; Promote black Pawn ?
         CMP     al,29
;        JR      NC,MP26                ; No - Jump
         JNC     MP26
;MP25:   LD      bx,P2                  ; Flag address
MP25:    MOV     ebx,offset P2
;        SET     5,(bx)                 ; Set promote flag
         LAHF
         OR      byte ptr [ebx],20h
         SAHF
;MP26:   CALL    ADMOVE                 ; Add to move list
MP26:    CALL    ADMOVE
;        INC     di                     ; Adjust to two square move
         LEA     edi,[edi+1]
;        DEC     ch
         DEC     ch
;        LD      bx,P1                  ; Check Pawn moved flag
         MOV     ebx,offset P1
;        BIT     3,(bx)                 ; Has it moved before ?
         TEST    byte ptr [ebx],8
;        JR      Z,MP10                 ; No - Jump
         JZ      MP10
;        JMP     MP15                   ; Jump
         JMP     MP15
;MP30:   EXAF                           ; Is forward square empty ?
MP30:    Z80_EXAF
;        JR      NZ,MP15                ; No - Jump
         JNZ     MP15
;MP31:   CALL    ADMOVE                 ; Add to move list
MP31:    CALL    ADMOVE
;        JMP     MP15                   ; Jump
         JMP     MP15
;MP35:   EXAF                           ; Is diagonal square empty ?
MP35:    Z80_EXAF
;        JR      Z,MP36                 ; Yes - Jump
         JZ      MP36
;        LD      al,(M2)                ; Get "to" position
         MOV     al,byte ptr [M2]
;        CMP     al,91                  ; Promote white Pawn ?
         CMP     al,91
;        JR      NC,MP37                ; Yes - Jump
         JNC     MP37
;        CMP     al,29                  ; Black Pawn promotion ?
         CMP     al,29
;        JR      NC,MP31                ; No- Jump
         JNC     MP31
;MP37:   LD      bx,P2                  ; Get flag address
MP37:    MOV     ebx,offset P2
;        SET     5,(bx)                 ; Set promote flag
         LAHF
         OR      byte ptr [ebx],20h
         SAHF
;        JR      MP31                   ; Jump
         JMP     MP31
;MP36:   CALL    ENPSNT                 ; Try en passant capture
MP36:    CALL    ENPSNT
;        JMP     MP15                   ; Jump
         JMP     MP15

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
;ENPSNT: LD      al,(M1)                ; Set position of Pawn
ENPSNT:  MOV     al,byte ptr [M1]
;        LD      bx,P1                  ; Check color
         MOV     ebx,offset P1
;        BIT     7,(bx)                 ; Is it white ?
         TEST    byte ptr [ebx],80h
;        JR      Z,rel002               ; Yes - skip
         JZ      rel002
;        ADD     al,10                  ; Add 10 for black
         ADD     al,10
;rel002: CMP     al,61                  ; On en passant capture rank ?
rel002:  CMP     al,61
;        RET     C                      ; No - return
         JNC     skip3
         RET
skip3:
;        CMP     al,69                  ; On en passant capture rank ?
         CMP     al,69
;        RET     NC                     ; No - return
         JC      skip4
         RET
skip4:
;        LD      si,(MLPTRJ)            ; Get pointer to previous move
         MOV     esi,[MLPTRJ]
;        BIT     4,(si+MLFLG)           ; First move for that piece ?
         TEST    byte ptr [esi+MLFLG],10h
;        RET     Z                      ; No - return
         JNZ     skip5
         RET
skip5:
;        LD      al,(si+MLTOP)          ; Get "to" postition
         MOV     al,byte ptr [esi+MLTOP]
;        LD      (M4),al                ; Store as index to board
         MOV     byte ptr [M4],al
;        LD      si,(M4)                ; Load board index
         MOV     esi,[M4]
;        LD      al,(si+BOARD)          ; Get piece moved
         MOV     al,byte ptr [esi+BOARD]
;        LD      (P3),al                ; Save it
         MOV     byte ptr [P3],al
;        AND     al,7                   ; Get piece type
         AND     al,7
;        CMP     al,PAWN                ; Is it a Pawn ?
         CMP     al,PAWN
;        RET     NZ                     ; No - return
         JZ      skip6
         RET
skip6:
;        LD      al,(M4)                ; Get "to" position
         MOV     al,byte ptr [M4]
;        LD      bx,M2                  ; Get present "to" position
         MOV     ebx,offset M2
;        SUB     al,(bx)                ; Find difference
         SUB     al,byte ptr [ebx]
;        JMP     P,rel003               ; Positive ? Yes - Jump
         JP      rel003
;        NEG                            ; Else take absolute value
         NEG     al
;        CMP     al,10                  ; Is difference 10 ?
         CMP     al,10
;rel003: RET     NZ                     ; No - return
rel003:  JZ      skip7
         RET
skip7:
;        LD      bx,P2                  ; Address of flags
         MOV     ebx,offset P2
;        SET     6,(bx)                 ; Set double move flag
         LAHF
         OR      byte ptr [ebx],40h
         SAHF
;        CALL    ADMOVE                 ; Add Pawn move to move list
         CALL    ADMOVE
;        LD      al,(M1)                ; Save initial Pawn position
         MOV     al,byte ptr [M1]
;        LD      (M3),al
         MOV     byte ptr [M3],al
;        LD      al,(M4)                ; Set "from" and "to" positions
         MOV     al,byte ptr [M4]
                        ; for dummy move
;X p33
;        LD      (M1),al
         MOV     byte ptr [M1],al
;        LD      (M2),al
         MOV     byte ptr [M2],al
;        LD      al,(P3)                ; Save captured Pawn
         MOV     al,byte ptr [P3]
;        LD      (P2),al
         MOV     byte ptr [P2],al
;        CALL    ADMOVE                 ; Add Pawn capture to move list
         CALL    ADMOVE
;        LD      al,(M3)                ; Restore "from" position
         MOV     al,byte ptr [M3]
;        LD      (M1),al
         MOV     byte ptr [M1],al

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
;ADJPTR: LD      bx,(MLLST)             ; Get list pointer
ADJPTR:  MOV     ebx,[MLLST]
;        LD      dx,-8                  ; Size of a move entry
         MOV     edx,-8
;        ADD     bx,dx                  ; Back up list pointer
         LEA     ebx,[ebx+edx]
;        LD      (MLLST),bx             ; Save list pointer
         MOV     [MLLST],ebx
;        LD      (bx),0                 ; Zero out link, first byte
;        INC     bx                     ; Next byte
;        LD      (bx),0                 ; Zero out link, second byte
         mov     dword ptr [ebx],0
;        RET                            ; Return
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
;CASTLE: LD      al,(P1)                ; Get King
CASTLE:  MOV     al,byte ptr [P1]
;        BIT     3,al                   ; Has it moved ?
         TEST    al,8
;        RET     NZ                     ; Yes - return
         JZ      skip8
         RET
skip8:
;        LD      al,(CKFLG)             ; Fetch Check Flag
         MOV     al,byte ptr [CKFLG]
;        AND     al,al                  ; Is the King in check ?
         AND     al,al
;        RET     NZ                     ; Yes - Return
         JZ      skip9
         RET
skip9:
;        LD      cx,0FF03H              ; Initialize King-side values
         MOV     ecx,0FF03H
;CA5:    LD      al,(M1)                ; King position
CA5:     MOV     al,byte ptr [M1]
;        ADD     al,cl                  ; Rook position
         ADD     al,cl
;        LD      cl,al                  ; Save
         MOV     cl,al
;        LD      (M3),al                ; Store as board index
         MOV     byte ptr [M3],al
;        LD      si,(M3)                ; Load board index
         MOV     esi,[M3]
;        LD      al,(si+BOARD)          ; Get contents of board
         MOV     al,byte ptr [esi+BOARD]
;        AND     al,7FH                 ; Clear color bit
         AND     al,7FH
;        CMP     al,ROOK                ; Has Rook ever moved ?
         CMP     al,ROOK
;        JR      NZ,CA20                ; Yes - Jump
         JNZ     CA20
;        LD      al,cl                  ; Restore Rook position
         MOV     al,cl
;        JR      CA15                   ; Jump
         JMP     CA15
;CA10:   LD      si,(M3)                ; Load board index
CA10:    MOV     esi,[M3]
;        LD      al,(si+BOARD)          ; Get contents of board
         MOV     al,byte ptr [esi+BOARD]
;        AND     al,al                  ; Empty ?
         AND     al,al
;        JR      NZ,CA20                ; No - Jump
         JNZ     CA20
;        LD      al,(M3)                ; Current position
         MOV     al,byte ptr [M3]
;        CMP     al,22                  ; White Queen Knight square ?
         CMP     al,22
;        JR      Z,CA15                 ; Yes - Jump
         JZ      CA15
;        CMP     al,92                  ; Black Queen Knight square ?
         CMP     al,92
;        JR      Z,CA15                 ; Yes - Jump
         JZ      CA15
;        CALL    ATTACK                 ; Look for attack on square
         CALL    ATTACK
;        AND     al,al                  ; Any attackers ?
         AND     al,al
;        JR      NZ,CA20                ; Yes - Jump
         JNZ     CA20
;        LD      al,(M3)                ; Current position
         MOV     al,byte ptr [M3]
;CA15:   ADD     al,ch                  ; Next position
CA15:    ADD     al,ch
;        LD      (M3),al                ; Save as board index
         MOV     byte ptr [M3],al
;        LD      bx,M1                  ; King position
         MOV     ebx,offset M1
;        CMP     al,(bx)                ; Reached King ?
         CMP     al,byte ptr [ebx]
;X p35
;        JR      NZ,CA10                ; No - jump
         JNZ     CA10
;        SUB     al,ch                  ; Determine King's position
         SUB     al,ch
;        SUB     al,ch
         SUB     al,ch
;        LD      (M2),al                ; Save it
         MOV     byte ptr [M2],al
;        LD      bx,P2                  ; Address of flags
         MOV     ebx,offset P2
;        LD      (bx),40H               ; Set double move flag
         MOV     byte ptr [ebx],40H
;        CALL    ADMOVE                 ; Put king move in list
         CALL    ADMOVE
;        LD      bx,M1                  ; Addr of King "from" position
         MOV     ebx,offset M1
;        LD      al,(bx)                ; Get King's "from" position
         MOV     al,byte ptr [ebx]
;        LD      (bx),cl                ; Store Rook "from" position
         MOV     byte ptr [ebx],cl
;        SUB     al,ch                  ; Get Rook "to" position
         SUB     al,ch
;        LD      (M2),al                ; Store Rook "to" position
         MOV     byte ptr [M2],al
;        XOR     al,al                  ; Zero
         XOR     al,al
;        LD      (P2),al                ; Zero move flags
         MOV     byte ptr [P2],al
;        CALL    ADMOVE                 ; Put Rook move in list
         CALL    ADMOVE
;        CALL    ADJPTR                 ; Re-adjust move list pointer
         CALL    ADJPTR
;        LD      al,(M3)                ; Restore King position
         MOV     al,byte ptr [M3]
;        LD      (M1),al                ; Store
         MOV     byte ptr [M1],al
;CA20:   LD      al,ch                  ; Scan Index
CA20:    MOV     al,ch
;        CMP     al,1                   ; Done ?
         CMP     al,1
;        RET     Z                      ; Yes - return
         JNZ     skip10
         RET
skip10:
;        LD      cx,01FCH               ; Set Queen-side initial values
         MOV     ecx,01FCH
;        JMP     CA5                    ; Jump
         JMP     CA5

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
;ADMOVE: LD      dx,(MLNXT)             ; Addr of next loc in move list
ADMOVE:  MOV     edx,[MLNXT]
;        LD      bx,MLEND               ; Address of list end
         MOV     ebx,offset MLEND
;        AND     al,al                  ; Clear carry flag
         AND     al,al
;        SBC     bx,dx                  ; Calculate difference
         SBB     ebx,edx
;        JR      C,AM10                 ; Jump if out of space
         JC      AM10
;        LD      bx,(MLLST)             ; Addr of prev. list area
         MOV     ebx,[MLLST]
;        LD      (MLLST),dx             ; Savn next as previous
         MOV     [MLLST],edx
;        LD      (bx),dl                ; Store link address
;        INC     bx
;        LD      (bx),dh
         MOV     dword ptr [ebx],edx
;        LD      bx,P1                  ; Address of moved piece
         MOV     ebx,offset P1
;        BIT     3,(bx)                 ; Has it moved before ?
         TEST    byte ptr [ebx],8
;        JR      NZ,rel004              ; Yes - jump
         JNZ     rel004
;        LD      bx,P2                  ; Address of move flags
         MOV     ebx,offset P2
;        SET     4,(bx)                 ; Set first move flag
         LAHF
         OR      byte ptr [ebx],10h
         SAHF
;rel004: EX      dx,bx                  ; Address of move area
rel004:  XCHG    ebx,edx
;        LD      (bx),0                 ; Store zero in link address
;        INC     bx
;        LD      (bx),0
;        INC     bx
         MOV     dword ptr [ebx],0
         LEA     ebx,[ebx+4]
;        LD      al,(M1)                ; Store "from" move position
         MOV     al,byte ptr [M1]
;        LD      (bx),al
         MOV     byte ptr [ebx],al
;        INC     bx
         LEA     ebx,[ebx+1]
;        LD      al,(M2)                ; Store "to" move position
         MOV     al,byte ptr [M2]
;        LD      (bx),al
         MOV     byte ptr [ebx],al
;        INC     bx
         LEA     ebx,[ebx+1]
;        LD      al,(P2)                ; Store move flags/capt. piece
         MOV     al,byte ptr [P2]
;        LD      (bx),al
         MOV     byte ptr [ebx],al
;        INC     bx
         LEA     ebx,[ebx+1]
;        LD      (bx),0                 ; Store initial move value
         MOV     byte ptr [ebx],0
;        INC     bx
         LEA     ebx,[ebx+1]
;        LD      (MLNXT),bx             ; Save address for next move
         MOV     [MLNXT],ebx
;        RET                            ; Return
         RET
;AM10:   LD      (bx),0                 ; Abort entry on table ovflow
AM10:    MOV     byte ptr [ebx],0
;        INC     bx
         LEA     ebx,[ebx+1]
;        LD      (bx),0
         MOV     byte ptr [ebx],0
;        DEC     bx
         LEA     ebx,[ebx-1]
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
;GENMOV: CALL    INCHK                  ; Test for King in check
GENMOV:  CALL    INCHK
;        LD      (CKFLG),al             ; Save attack count as flag
         MOV     byte ptr [CKFLG],al
;        LD      dx,(MLNXT)             ; Addr of next avail list space
         MOV     edx,[MLNXT]
;        LD      bx,(MLPTRI)            ; Ply list pointer index
         MOV     ebx,[MLPTRI]
;        INC     bx                     ; Increment to next ply
         LEA     ebx,[ebx+1]
;        INC     bx
         LEA     ebx,[ebx+1]
;        LD      (bx),dl                ; Save move list pointer
         MOV     byte ptr [ebx],dl
;        INC     bx
         LEA     ebx,[ebx+1]
;        LD      (bx),dh
         MOV     byte ptr [ebx],dh
;        INC     bx
         LEA     ebx,[ebx+1]
;        LD      (MLPTRI),bx            ; Save new index
         MOV     [MLPTRI],ebx
;        LD      (MLLST),bx             ; Last pointer for chain init.
         MOV     [MLLST],ebx
;        LD      al,21                  ; First position on board
         MOV     al,21
;GM5:    LD      (M1),al                ; Save as index
GM5:     MOV     byte ptr [M1],al
;        LD      si,(M1)                ; Load board index
         MOV     esi,[M1]
;        LD      al,(si+BOARD)          ; Fetch board contents
         MOV     al,byte ptr [esi+BOARD]
;        AND     al,al                  ; Is it empty ?
         AND     al,al
;        JR      Z,GM10                 ; Yes - Jump
         JZ      GM10
;        CMP     al,-1                  ; Is it a boarder square ?
         CMP     al,-1
;        JR      Z,GM10                 ; Yes - Jump
         JZ      GM10
;        LD      (P1),al                ; Save piece
         MOV     byte ptr [P1],al
;        LD      bx,COLOR               ; Address of color of piece
         MOV     ebx,offset COLOR
;        XOR     al,(bx)                ; Test color of piece
         XOR     al,byte ptr [ebx]
;        BIT     7,al                   ; Match ?
         TEST    al,80h
;        CALL    Z,MPIECE               ; Yes - call Move Piece
         JNZ     skip11
         CALL    MPIECE
skip11:
;GM10:   LD      al,(M1)                ; Fetch current board position
GM10:    MOV     al,byte ptr [M1]
;        INC     al                     ; Incr to next board position
         INC     al
;        CMP     al,99                  ; End of board array ?
         CMP     al,99
;        JMP     NZ,GM5                 ; No - Jump
         JNZ     GM5
;        RET                            ; Return
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
;INCHK:  LD      al,(COLOR)             ; Get color
INCHK:   MOV     al,byte ptr [COLOR]
;INCHK1: LD      bx,POSK                ; Addr of white King position
INCHK1:  MOV     ebx,offset POSK
;        AND     al,al                  ; White ?
         AND     al,al
;        JR      Z,rel005               ; Yes - Skip
         JZ      rel005
;        INC     bx                     ; Addr of black King position
         LEA     ebx,[ebx+1]
;rel005: LD      al,(bx)                ; Fetch King position
rel005:  MOV     al,byte ptr [ebx]
;        LD      (M3),al                ; Save
         MOV     byte ptr [M3],al
;        LD      si,(M3)                ; Load board index
         MOV     esi,[M3]
;        LD      al,(si+BOARD)          ; Fetch board contents
         MOV     al,byte ptr [esi+BOARD]
;        LD      (P1),al                ; Save
         MOV     byte ptr [P1],al
;        AND     al,7                   ; Get piece type
         AND     al,7
;        LD      (T1),al                ; Save
         MOV     byte ptr [T1],al
;        CALL    ATTACK                 ; Look for attackers on King
         CALL    ATTACK
;        RET                            ; Return
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
;ATTACK: PUSH    cx                     ; Save Register B
ATTACK:  PUSH ecx
;        XOR     al,al                  ; Clear
         XOR     al,al
;        LD      ch,16                  ; Initial direction count
         MOV     ch,16
;        LD      (INDX2),al             ; Initial direction index
         MOV     byte ptr [INDX2],al
;        LD      di,(INDX2)             ; Load index
         MOV     edi,[INDX2]
;AT5:    LD      cl,(di+DIRECT)         ; Get direction
AT5:     MOV     cl,byte ptr [edi+DIRECT]
;        LD      dh,0                   ; Init. scan count/flags
         MOV     dh,0
;        LD      al,(M3)                ; Init. board start position
         MOV     al,byte ptr [M3]
;        LD      (M2),al                ; Save
         MOV     byte ptr [M2],al
;AT10:   INC     dh                     ; Increment scan count
AT10:    INC     dh
;        CALL    PATH                   ; Next position
         CALL    PATH
;        CMP     al,1                   ; Piece of a opposite color ?
         CMP     al,1
;        JR      Z,AT14A                ; Yes - jump
         JZ      AT14A
;        CMP     al,2                   ; Piece of same color ?
         CMP     al,2
;        JR      Z,AT14B                ; Yes - jump
         JZ      AT14B
;        AND     al,al                  ; Empty position ?
         AND     al,al
;        JR      NZ,AT12                ; No - jump
         JNZ     AT12
;        LD      al,ch                  ; Fetch direction count
         MOV     al,ch
;        CMP     al,9                   ; On knight scan ?
         CMP     al,9
;        JR      NC,AT10                ; No - jump
         JNC     AT10
;AT12:   INC     di                     ; Increment direction index
AT12:    LEA     edi,[edi+1]
;        DJNZ    AT5                    ; Done ? No - jump
         LAHF
         DEC ch
         JNZ     AT5
         SAHF
;        XOR     al,al                  ; No attackers
         XOR     al,al
;AT13:   POP     cx                     ; Restore register B
AT13:    POP ecx
;        RET                            ; Return
         RET
;AT14A:  BIT     6,dh                   ; Same color found already ?
AT14A:   TEST    dh,40h
;        JR      NZ,AT12                ; Yes - jump
         JNZ     AT12
;        SET     5,dh                   ; Set opposite color found flag
         LAHF
         OR      dh,20h
         SAHF
;        JMP     AT14                   ; Jump
         JMP     AT14
;AT14B:  BIT     5,dh                   ; Opposite color found already?
AT14B:   TEST    dh,20h
;        JR      NZ,AT12                ; Yes - jump
         JNZ     AT12
;        SET     6,dh                   ; Set same color found flag
         LAHF
         OR      dh,40h
         SAHF

; ***** DETERMINE IF PIECE ENCOUNTERED ATTACKS SQUARE *****
;AT14:   LD      al,(T2)                ; Fetch piece type encountered
AT14:    MOV     al,byte ptr [T2]
;        LD      dl,al                  ; Save
         MOV     dl,al
;        LD      al,ch                  ; Get direction-counter
         MOV     al,ch
;        CMP     al,9                   ; Look for Knights ?
         CMP     al,9
;        JR      C,AT25                 ; Yes - jump
         JC      AT25
;        LD      al,dl                  ; Get piece type
         MOV     al,dl
;        CMP     al,QUEEN               ; Is is a Queen ?
         CMP     al,QUEEN
;        JR      NZ,AT15                ; No - Jump
         JNZ     AT15
;        SET     7,dh                   ; Set Queen found flag
         LAHF
         OR      dh,80h
         SAHF
;        JR      AT30                   ; Jump
         JMP     AT30
;AT15:   LD      al,dh                  ; Get flag/scan count
AT15:    MOV     al,dh
;        AND     al,0FH                 ; Isolate count
         AND     al,0FH
;        CMP     al,1                   ; On first position ?
         CMP     al,1
;        JR      NZ,AT16                ; No - jump
         JNZ     AT16
;        LD      al,dl                  ; Get encountered piece type
         MOV     al,dl
;        CMP     al,KING                ; Is it a King ?
         CMP     al,KING
;        JR      Z,AT30                 ; Yes - jump
         JZ      AT30
;AT16:   LD      al,ch                  ; Get direction counter
AT16:    MOV     al,ch
;        CMP     al,13                  ; Scanning files or ranks ?
         CMP     al,13
;        JR      C,AT21                 ; Yes - jump
         JC      AT21
;        LD      al,dl                  ; Get piece type
         MOV     al,dl
;        CMP     al,BISHOP              ; Is it a Bishop ?
         CMP     al,BISHOP
;        JR      Z,AT30                 ; Yes - jump
         JZ      AT30
;        LD      al,dh                  ; Get flags/scan count
         MOV     al,dh
;        AND     al,0FH                 ; Isolate count
         AND     al,0FH
;        CMP     al,1                   ; On first position ?
         CMP     al,1
;        JR      NZ,AT12                ; No - jump
         JNZ     AT12
;        CMP     al,dl                  ; Is it a Pawn ?
         CMP     al,dl
;        JR      NZ,AT12                ; No - jump
         JNZ     AT12
;        LD      al,(P2)                ; Fetch piece including color
         MOV     al,byte ptr [P2]
;X p41
;        BIT     7,al                   ; Is it white ?
         TEST    al,80h
;        JR      Z,AT20                 ; Yes - jump
         JZ      AT20
;        LD      al,ch                  ; Get direction counter
         MOV     al,ch
;        CMP     al,15                  ; On a non-attacking diagonal ?
         CMP     al,15
;        JR      C,AT12                 ; Yes - jump
         JC      AT12
;        JR      AT30                   ; Jump
         JMP     AT30
;AT20:   LD      al,ch                  ; Get direction counter
AT20:    MOV     al,ch
;        CMP     al,15                  ; On a non-attacking diagonal ?
         CMP     al,15
;        JR      NC,AT12                ; Yes - jump
         JNC     AT12
;        JR      AT30                   ; Jump
         JMP     AT30
;AT21:   LD      al,dl                  ; Get piece type
AT21:    MOV     al,dl
;        CMP     al,ROOK                ; Is is a Rook ?
         CMP     al,ROOK
;        JR      NZ,AT12                ; No - jump
         JNZ     AT12
;        JR      AT30                   ; Jump
         JMP     AT30
;AT25:   LD      al,dl                  ; Get piece type
AT25:    MOV     al,dl
;        CMP     al,KNIGHT              ; Is it a Knight ?
         CMP     al,KNIGHT
;        JR      NZ,AT12                ; No - jump
         JNZ     AT12
;AT30:   LD      al,(T1)                ; Attacked piece type/flag
AT30:    MOV     al,byte ptr [T1]
;        CMP     al,7                   ; Call from POINTS ?
         CMP     al,7
;        JR      Z,AT31                 ; Yes - jump
         JZ      AT31
;        BIT     5,dh                   ; Is attacker opposite color ?
         TEST    dh,20h
;        JR      Z,AT32                 ; No - jump
         JZ      AT32
;        LD      al,1                   ; Set attacker found flag
         MOV     al,1
;        JMP     AT13                   ; Jump
         JMP     AT13
;AT31:   CALL    ATKSAV                 ; Save attacker in attack list
AT31:    CALL    ATKSAV
;AT32:   LD      al,(T2)                ; Attacking piece type
AT32:    MOV     al,byte ptr [T2]
;        CMP     al,KING                ; Is it a King,?
         CMP     al,KING
;        JMP     Z,AT12                 ; Yes - jump
         JZ      AT12
;        CMP     al,KNIGHT              ; Is it a Knight ?
         CMP     al,KNIGHT
;        JMP     Z,AT12                 ; Yes - jump
         JZ      AT12
;        JMP     AT10                   ; Jump
         JMP     AT10
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
;ATKSAV: PUSH    cx                     ; Save Regs BC
ATKSAV:  PUSH ecx
;        PUSH    dx                     ; Save Regs DE
         PUSH edx
;        LD      al,(NPINS)             ; Number of pinned pieces
         MOV     al,byte ptr [NPINS]
;        AND     al,al                  ; Any ?
         AND     al,al
;        CALL    NZ,PNCK                ; yes - check pin list
         JZ      skip12
         CALL    PNCK
skip12:
;        LD      si,(T2)                ; Init index to value table
         MOV     esi,[T2]
;        LD      bx,ATKLST              ; Init address of attack list
         MOV     ebx,offset ATKLST
;        LD      cx,0                   ; Init increment for white
         MOV     ecx,0
;        LD      al,(P2)                ; Attacking piece
         MOV     al,byte ptr [P2]
;        BIT     7,al                   ; Is it white ?
         TEST    al,80h
;        JR      Z,rel006               ; Yes - jump
         JZ      rel006
;        LD      cl,7                   ; Init increment for black
         MOV     cl,7
;rel006: AND     al,7                   ; Attacking piece type
rel006:  AND     al,7
;        LD      dl,al                  ; Init increment for type
         MOV     dl,al
;        BIT     7,dh                   ; Queen found this scan ?
         TEST    dh,80h
;        JR      Z,rel007               ; No - jump
         JZ      rel007
;        LD      dl,QUEEN               ; Use Queen slot in attack list
         MOV     dl,QUEEN
;rel007: ADD     bx,cx                  ; Attack list address
rel007:  LEA     ebx,[ebx+ecx]
;        INC     (bx)                   ; Increment list count
         INC     byte ptr [ebx]
;        LD      dh,0
         MOV     dh,0
;        ADD     bx,dx                  ; Attack list slot address
         LEA     ebx,[ebx+edx]
;        LD      al,(bx)                ; Get data already there
         MOV     al,byte ptr [ebx]
;        AND     al,0FH                 ; Is first slot empty ?
         AND     al,0FH
;        JR      Z,AS20                 ; Yes - jump
         JZ      AS20
;        LD      al,(bx)                ; Get data again
         MOV     al,byte ptr [ebx]
;        AND     al,0F0H                ; Is second slot empty ?
         AND     al,0F0H
;        JR      Z,AS19                 ; Yes - jump
         JZ      AS19
;        INC     bx                     ; Increment to King slot
         LEA     ebx,[ebx+1]
;        JR      AS20                   ; Jump
         JMP     AS20
;AS19:   RLD                            ; Temp save lower in upper
AS19:    Z80_RLD
;        LD      al,(si+PVALUE)         ; Get new value for attack list
         MOV     al,byte ptr [esi+PVALUE]
;        RRD                            ; Put in 2nd attack list slot
         Z80_RRD
;        JR      AS25                   ; Jump
         JMP     AS25
;AS20:   LD      al,(si+PVALUE)         ; Get new value for attack list
AS20:    MOV     al,byte ptr [esi+PVALUE]
;        RLD                            ; Put in 1st attack list slot
         Z80_RLD
;X p43
;AS25:   POP     dx                     ; Restore DE regs
AS25:    POP edx
;        POP     cx                     ; Restore BC regs
         POP ecx
;        RET                            ; Return
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
;PNCK:   LD      dh,cl                  ; Save attack direction
PNCK:    MOV     dh,cl
;        LD      dl,0                   ; Clear flag
         MOV     dl,0
;        LD      cl,al                  ; Load pin count for search
         MOV     cl,al
;        LD      ch,0
         MOV     ch,0
;        LD      al,(M2)                ; Position of piece
         MOV     al,byte ptr [M2]
;        LD      bx,PLISTA              ; Pin list address
         MOV     ebx,offset PLISTA
;PC1:    CCIR                           ; Search list for position
PC1:     CCIR
;        RET     NZ                     ; Return if not found
         JZ      skip13
         RET
skip13:
;        EXAF                           ; Save search paramenters
         Z80_EXAF
;        BIT     0,dl                   ; Is this the first find ?
         TEST    dl,1
;        JR      NZ,PC5                 ; No - jump
         JNZ     PC5
;        SET     0,dl                   ; Set first find flag
         LAHF
         OR      dl,1
         SAHF
;        PUSH    bx                     ; Get corresp index to dir list
         PUSH ebx
;        POP     si
         POP esi
;        LD      al,(si+9)              ; Get direction
         MOV     al,byte ptr [esi+9]
;        CMP     al,dh                  ; Same as attacking direction ?
         CMP     al,dh
;        JR      Z,PC3                  ; Yes - jump
         JZ      PC3
;        NEG                            ; Opposite direction ?
         NEG     al
;        CMP     al,dh                  ; Same as attacking direction ?
         CMP     al,dh
;        JR      NZ,PC5                 ; No - jump
         JNZ     PC5
;PC3:    EXAF                           ; Restore search parameters
PC3:     Z80_EXAF
;        JMP     PE,PC1                 ; Jump if search not complete
         JPE     PC1
;        RET                            ; Return
         RET
;PC5:    POP     af                     ; Abnormal exit
PC5:     POP eax
         sahf
;        POP     dx                     ; Restore regs.
         POP edx
;        POP     cx
         POP ecx
;        RET                            ; Return to ATTACK
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
;PINFND: XOR     al,al                  ; Zero pin count
PINFND:  XOR     al,al
;        LD      (NPINS),al
         MOV     byte ptr [NPINS],al
;        LD      dx,POSK                ; Addr of King/Queen pos list
         MOV     edx,offset POSK
;PF1:    LD      al,(dx)                ; Get position of royal piece
PF1:     MOV     al,[edx]
;        AND     al,al                  ; Is it on board ?
         AND     al,al
;        JMP     Z,PF26                 ; No- jump
         JZ      PF26
;        CMP     al,-1                  ; At end of list ?
         CMP     al,-1
;        RET     Z                      ; Yes return
         JNZ     skip14
         RET
skip14:
;        LD      (M3),al                ; Save position as board index
         MOV     byte ptr [M3],al
;        LD      si,(M3)                ; Load index to board
         MOV     esi,[M3]
;        LD      al,(si+BOARD)          ; Get contents of board
         MOV     al,byte ptr [esi+BOARD]
;        LD      (P1),al                ; Save
         MOV     byte ptr [P1],al
;        LD      ch,8                   ; Init scan direction count
         MOV     ch,8
;        XOR     al,al
         XOR     al,al
;        LD      (INDX2),al             ; Init direction index
         MOV     byte ptr [INDX2],al
;        LD      di,(INDX2)
         MOV     edi,[INDX2]
;PF2:    LD      al,(M3)                ; Get King/Queen position
PF2:     MOV     al,byte ptr [M3]
;        LD      (M2),al                ; Save
         MOV     byte ptr [M2],al
;        XOR     al,al
         XOR     al,al
;        LD      (M4),al                ; Clear pinned piece saved pos
         MOV     byte ptr [M4],al
;        LD      cl,(di+DIRECT)         ; Get direction of scan
         MOV     cl,byte ptr [edi+DIRECT]
;PF5:    CALL    PATH                   ; Compute next position
PF5:     CALL    PATH
;        AND     al,al                  ; Is it empty ?
         AND     al,al
;        JR      Z,PF5                  ; Yes - jump
         JZ      PF5
;        CMP     al,3                   ; Off board ?
         CMP     al,3
;        JMP     Z,PF25                 ; Yes - jump
         JZ      PF25
;        CMP     al,2                   ; Piece of same color
         CMP     al,2
;        LD      al,(M4)                ; Load pinned piece position
         MOV     al,byte ptr [M4]
;        JR      Z,PF15                 ; Yes - jump
         JZ      PF15
;        AND     al,al                  ; Possible pin ?
         AND     al,al
;        JMP     Z,PF25                 ; No - jump
         JZ      PF25
;        LD      al,(T2)                ; Piece type encountered
         MOV     al,byte ptr [T2]
;        CMP     al,QUEEN               ; Queen ?
         CMP     al,QUEEN
;        JMP     Z,PF19                 ; Yes - jump
         JZ      PF19
;        LD      bl,al                  ; Save piece type
         MOV     bl,al
;X p45
;        LD      al,ch                  ; Direction counter
         MOV     al,ch
;        CMP     al,5                   ; Non-diagonal direction ?
         CMP     al,5
;        JR      C,PF10                 ; Yes - jump
         JC      PF10
;        LD      al,bl                  ; Piece type
         MOV     al,bl
;        CMP     al,BISHOP              ; Bishop ?
         CMP     al,BISHOP
;        JMP     NZ,PF25                ; No - jump
         JNZ     PF25
;        JMP     PF20                   ; Jump
         JMP     PF20
;PF10:   LD      al,bl                  ; Piece type
PF10:    MOV     al,bl
;        CMP     al,ROOK                ; Rook ?
         CMP     al,ROOK
;        JMP     NZ,PF25                ; No - jump
         JNZ     PF25
;        JMP     PF20                   ; Jump
         JMP     PF20
;PF15:   AND     al,al                  ; Possible pin ?
PF15:    AND     al,al
;        JMP     NZ,PF25                ; No - jump
         JNZ     PF25
;        LD      al,(M2)                ; Save possible pin position
         MOV     al,byte ptr [M2]
;        LD      (M4),al
         MOV     byte ptr [M4],al
;        JMP     PF5                    ; Jump
         JMP     PF5
;PF19:   LD      al,(P1)                ; Load King or Queen
PF19:    MOV     al,byte ptr [P1]
;        AND     al,7                   ; Clear flags
         AND     al,7
;        CMP     al,QUEEN               ; Queen ?
         CMP     al,QUEEN
;        JR      NZ,PF20                ; No - jump
         JNZ     PF20
;        PUSH    cx                     ; Save regs.
         PUSH ecx
;        PUSH    dx
         PUSH edx
;        PUSH    di
         PUSH edi
;        XOR     al,al                  ; Zero out attack list
         XOR     al,al
;        LD      ch,14
         MOV     ch,14
;        LD      bx,ATKLST
         MOV     ebx,offset ATKLST
;back02: LD      (bx),al
back02:  MOV     byte ptr [ebx],al
;        INC     bx
         LEA     ebx,[ebx+1]
;        DJNZ    back02
         LAHF
         DEC ch
         JNZ     back02
         SAHF
;        LD      al,7                   ; Set attack flag
         MOV     al,7
;        LD      (T1),al
         MOV     byte ptr [T1],al
;        CALL    ATTACK                 ; Find attackers/defenders
         CALL    ATTACK
;        LD      bx,WACT                ; White queen attackers
         MOV     ebx,WACT
;        LD      dx,BACT                ; Black queen attackers
         MOV     edx,BACT
;        LD      al,(P1)                ; Get queen
         MOV     al,byte ptr [P1]
;        BIT     7,al                   ; Is she white ?
         TEST    al,80h
;        JR      Z,rel008               ; Yes - skip
         JZ      rel008
;        EX      dx,bx                  ; Reverse for black
         XCHG    ebx,edx
;rel008: LD      al,(bx)                ; Number of defenders
rel008:  MOV     al,byte ptr [ebx]
;        EX      dx,bx                  ; Reverse for attackers
         XCHG    ebx,edx
;        SUB     al,(bx)                ; Defenders minus attackers
         SUB     al,byte ptr [ebx]
;        DEC     al                     ; Less 1
         DEC     al
;        POP     di                     ; Restore regs.
         POP edi
;        POP     dx
         POP edx
;        POP     cx
         POP ecx
;        JMP     P,PF25                 ; Jump if pin not valid
         JP      PF25
;PF20:   LD      bx,NPINS               ; Address of pinned piece count
PF20:    MOV     ebx,offset NPINS
;        INC     (bx)                   ; Increment
         INC     byte ptr [ebx]
;        LD      si,(NPINS)             ; Load pin list index
         MOV     esi,[NPINS]
;        LD      (si+PLISTD),cl         ; Save direction of pin
         MOV     byte ptr [esi+PLISTD],cl
;X p46
;        LD      al,(M4)                ; Position of pinned piece
         MOV     al,byte ptr [M4]
;        LD      (si+PLIST),al          ; Save in list
         MOV     byte ptr [esi+PLIST],al
;PF25:   INC     di                     ; Increment direction index
PF25:    LEA     edi,[edi+1]
;        DJNZ    PF27                   ; Done ? No - Jump
         LAHF
         DEC ch
         JNZ     PF27
         SAHF
;PF26:   INC     dx                     ; Incr King/Queen pos index
PF26:    LEA     edx,[edx+1]
;        JMP     PF1                    ; Jump
         JMP     PF1
;PF27:   JMP     PF2                    ; Jump
PF27:    JMP     PF2

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
;XCHNG:  EXX                            ; Swap regs.
XCHNG:   Z80_EXX
;        LD      al,(P1)                ; Piece attacked
         MOV     al,byte ptr [P1]
;        LD      bx,WACT                ; Addr of white attkrs/dfndrs
         MOV     ebx,WACT
;        LD      dx,BACT                ; Addr of black attkrs/dfndrs
         MOV     edx,BACT
;        BIT     7,al                   ; Is piece white ?
         TEST    al,80h
;        JR      Z,rel009               ; Yes - jump
         JZ      rel009
;        EX      dx,bx                  ; Swap list pointers
         XCHG    ebx,edx
;rel009: LD      ch,(bx)                ; Init list counts
rel009:  MOV     ch,byte ptr [ebx]
;        EX      dx,bx
         XCHG    ebx,edx
;        LD      cl,(bx)
         MOV     cl,byte ptr [ebx]
;        EX      dx,bx
         XCHG    ebx,edx
;        EXX                            ; Restore regs.
         Z80_EXX
;        LD      cl,0                   ; Init attacker/defender flag
         MOV     cl,0
;        LD      dl,0                   ; Init points lost count
         MOV     dl,0
;        LD      si,(T3)                ; Load piece value index
         MOV     esi,[T3]
;        LD      dh,(si+PVALUE)         ; Get attacked piece value
         MOV     dh,byte ptr [esi+PVALUE]
;        SLA     dh                     ; Double it
         SHL     dh,1
;        LD      ch,dh                  ; Save
         MOV     ch,dh
;        CALL    NEXTAD                 ; Retrieve first attacker
         CALL    NEXTAD
;        RET     Z                      ; Return if none
         JNZ     skip15
         RET
skip15:
;XC10:   LD      bl,al                  ; Save attacker value
XC10:    MOV     bl,al
;        CALL    NEXTAD                 ; Get next defender
         CALL    NEXTAD
;        JR      Z,XC18                 ; Jump if none
         JZ      XC18
;        EXAF                           ; Save defender value
         Z80_EXAF
;        LD      al,ch                  ; Get attacked value
         MOV     al,ch
;        CMP     al,bl                  ; Attacked less than attacker ?
         CMP     al,bl
;        JR      NC,XC19                ; No - jump
         JNC     XC19
;        EXAF                           ; -Restore defender
         Z80_EXAF
;XC15:   CMP     al,bl                  ; Defender less than attacker ?
XC15:    CMP     al,bl
;        RET     C                      ; Yes - return
         JNC     skip16
         RET
skip16:
;        CALL    NEXTAD                 ; Retrieve next attacker value
         CALL    NEXTAD
;        RET     Z                      ; Return if none
         JNZ     skip17
         RET
skip17:
;        LD      bl,al                  ; Save attacker value
         MOV     bl,al
;        CALL    NEXTAD                 ; Retrieve next defender value
         CALL    NEXTAD
;        JR      NZ,XC15                ; Jump if none
         JNZ     XC15
;XC18:   EXAF                           ; Save Defender
XC18:    Z80_EXAF
;        LD      al,ch                  ; Get value of attacked piece
         MOV     al,ch
;X p48
;XC19:   BIT     0,cl                   ; Attacker or defender ?
XC19:    TEST    cl,1
;        JR      Z,rel010               ; Jump if defender
         JZ      rel010
;        NEG                            ; Negate value for attacker
         NEG     al
;        ADD     al,dl                  ; Total points lost
         ADD     al,dl
;rel010: LD      dl,al                  ; Save total
rel010:  MOV     dl,al
;        EXAF                           ; Restore previous defender
         Z80_EXAF
;        RET     Z                      ; Return if none
         JNZ     skip18
         RET
skip18:
;        LD      ch,bl                  ; Prev attckr becomes defender
         MOV     ch,bl
;        JMP     XC10                   ; Jump
         JMP     XC10

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
;NEXTAD: INC     cl                     ; Increment side flag
NEXTAD:  INC     cl
;        EXX                            ; Swap registers
         Z80_EXX
;        LD      al,ch                  ; Swap list counts
         MOV     al,ch
;        LD      ch,cl
         MOV     ch,cl
;        LD      cl,al
         MOV     cl,al
;        EX      dx,bx                  ; Swap list pointers
         XCHG    ebx,edx
;        XOR     al,al
         XOR     al,al
;        CMP     al,ch                  ; At end of list ?
         CMP     al,ch
;        JR      Z,NX6                  ; Yes - jump
         JZ      NX6
;        DEC     ch                     ; Decrement list count
         DEC     ch
;back03: INC     bx                     ; Increment list inter
back03:  LEA     ebx,[ebx+1]
;        CMP     al,(bx)                ; Check next item in list
         CMP     al,byte ptr [ebx]
;        JR      Z,back03               ; Jump if empty
         JZ      back03
;        RRD                            ; Get value from list
         Z80_RRD
;        ADD     al,al                  ; Double it
         ADD     al,al
;        DEC     bx                     ; Decrement list pointer
         LEA     ebx,[ebx-1]
;NX6:    EXX                            ; Restore regs.
NX6:     Z80_EXX
;        RET                            ; Return
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
;POINTS: XOR     al,al                  ; Zero out variables
POINTS:  XOR     al,al
;        LD      (MTRL),al
         MOV     byte ptr [MTRL],al
;        LD      (BRDC),al
         MOV     byte ptr [BRDC],al
;        LD      (PTSL),al
         MOV     byte ptr [PTSL],al
;        LD      (PTSW1),al
         MOV     byte ptr [PTSW1],al
;        LD      (PTSW2),al
         MOV     byte ptr [PTSW2],al
;        LD      (PTSCK),al
         MOV     byte ptr [PTSCK],al
;        LD      bx,T1                  ; Set attacker flag
         MOV     ebx,offset T1
;        LD      (bx),7
         MOV     byte ptr [ebx],7
;        LD      al,21                  ; Init to first square on board
         MOV     al,21
;PT5:    LD      (M3),al                ; Save as board index
PT5:     MOV     byte ptr [M3],al
;        LD      si,(M3)                ; Load board index
         MOV     esi,[M3]
;        LD      al,(si+BOARD)          ; Get piece from board
         MOV     al,byte ptr [esi+BOARD]
;        CMP     al,-1                  ; Off board edge ?
         CMP     al,-1
;        JMP     Z,PT25                 ; Yes - jump
         JZ      PT25
;        LD      bx,P1                  ; Save piece, if any
         MOV     ebx,offset P1
;        LD      (bx),al
         MOV     byte ptr [ebx],al
;        AND     al,7                   ; Save piece type, if any
         AND     al,7
;        LD      (T3),al
         MOV     byte ptr [T3],al
;        CMP     al,KNIGHT              ; Less than a Knight (Pawn) ?
         CMP     al,KNIGHT
;        JR      C,PT6X                 ; Yes - Jump
         JC      PT6X
;        CMP     al,ROOK                ; Rook, Queen or King ?
         CMP     al,ROOK
;        JR      C,PT6B                 ; No - jump
         JC      PT6B
;        CMP     al,KING                ; Is it a King ?
         CMP     al,KING
;        JR      Z,PT6AA                ; Yes - jump
         JZ      PT6AA
;        LD      al,(MOVENO)            ; Get move number
         MOV     al,byte ptr [MOVENO]
;        CMP     al,7                   ; Less than 7 ?
         CMP     al,7
;        JR      C,PT6A                 ; Yes - Jump
         JC      PT6A
;        JMP     PT6X                   ; Jump
         JMP     PT6X
;PT6AA:  BIT     4,(bx)                 ; Castled yet ?
PT6AA:   TEST    byte ptr [ebx],10h
;        JR      Z,PT6A                 ; No - jump
         JZ      PT6A
;        LD      al,+6                  ; Bonus for castling
         MOV     al,+6
;        BIT     7,(bx)                 ; Check piece color
         TEST    byte ptr [ebx],80h
;        JR      Z,PT6D                 ; Jump if white
         JZ      PT6D
;        LD      al,-6                  ; Bonus for black castling
         MOV     al,-6
;X p50
;        JMP     PT6D                   ; Jump
         JMP     PT6D
;PT6A:   BIT     3,(bx)                 ; Has piece moved yet ?
PT6A:    TEST    byte ptr [ebx],8
;        JR      Z,PT6X                 ; No - jump
         JZ      PT6X
;        JMP     PT6C                   ; Jump
         JMP     PT6C
;PT6B:   BIT     3,(bx)                 ; Has piece moved yet ?
PT6B:    TEST    byte ptr [ebx],8
;        JR      NZ,PT6X                ; Yes - jump
         JNZ     PT6X
;PT6C:   LD      al,-2                  ; Two point penalty for white
PT6C:    MOV     al,-2
;        BIT     7,(bx)                 ; Check piece color
         TEST    byte ptr [ebx],80h
;        JR      Z,PT6D                 ; Jump if white
         JZ      PT6D
;        LD      al,+2                  ; Two point penalty for black
         MOV     al,+2
;PT6D:   LD      bx,BRDC                ; Get address of board control
PT6D:    MOV     ebx,offset BRDC
;        ADD     al,(bx)                ; Add on penalty/bonus points
         ADD     al,byte ptr [ebx]
;        LD      (bx),al                ; Save
         MOV     byte ptr [ebx],al
;PT6X:   XOR     al,al                  ; Zero out attack list
PT6X:    XOR     al,al
;        LD      ch,14
         MOV     ch,14
;        LD      bx,ATKLST
         MOV     ebx,offset ATKLST
;back04: LD      (bx),al
back04:  MOV     byte ptr [ebx],al
;        INC     bx
         LEA     ebx,[ebx+1]
;        DJNZ    back04
         LAHF
         DEC ch
         JNZ     back04
         SAHF
;        CALL    ATTACK                 ; Build attack list for square
         CALL    ATTACK
;        LD      bx,BACT                ; Get black attacker count addr
         MOV     ebx,BACT
;        LD      al,(WACT)              ; Get white attacker count
         MOV     al,byte ptr [WACT]
;        SUB     al,(bx)                ; Compute count difference
         SUB     al,byte ptr [ebx]
;        LD      bx,BRDC                ; Address of board control
         MOV     ebx,offset BRDC
;        ADD     al,(bx)                ; Accum board control score
         ADD     al,byte ptr [ebx]
;        LD      (bx),al                ; Save
         MOV     byte ptr [ebx],al
;        LD      al,(P1)                ; Get piece on current square
         MOV     al,byte ptr [P1]
;        AND     al,al                  ; Is it empty ?
         AND     al,al
;        JMP     Z,PT25                 ; Yes - jump
         JZ      PT25
;        CALL    XCHNG                  ; Evaluate exchange, if any
         CALL    XCHNG
;        XOR     al,al                  ; Check for a loss
         XOR     al,al
;        CMP     al,dl                  ; Points lost ?
         CMP     al,dl
;        JR      Z,PT23                 ; No - Jump
         JZ      PT23
;        DEC     dh                     ; Deduct half a Pawn value
         DEC     dh
;        LD      al,(P1)                ; Get piece under attack
         MOV     al,byte ptr [P1]
;        LD      bx,COLOR               ; Color of side just moved
         MOV     ebx,offset COLOR
;        XOR     al,(bx)                ; Compare with piece
         XOR     al,byte ptr [ebx]
;        BIT     7,al                   ; Do colors match ?
         TEST    al,80h
;        LD      al,dl                  ; Points lost
         MOV     al,dl
;        JR      NZ,PT20                ; Jump if no match
         JNZ     PT20
;        LD      bx,PTSL                ; Previous max points lost
         MOV     ebx,offset PTSL
;        CMP     al,(bx)                ; Compare to current value
         CMP     al,byte ptr [ebx]
;        JR      C,PT23                 ; Jump if greater than
         JC      PT23
;        LD      (bx),dl                ; Store new value as max lost
         MOV     byte ptr [ebx],dl
;        LD      si,(MLPTRJ)            ; Load pointer to this move
         MOV     esi,[MLPTRJ]
;        LD      al,(M3)                ; Get position of lost piece
         MOV     al,byte ptr [M3]
;        CMP     al,(si+MLTOP)          ; Is it the one moving ?
         CMP     al,byte ptr [esi+MLTOP]
;        JR      NZ,PT23                ; No - jump
         JNZ     PT23
;        LD      (PTSCK),al             ; Save position as a flag
         MOV     byte ptr [PTSCK],al
;        JMP     PT23                   ; Jump
         JMP     PT23
;X p51
;PT20:   LD      bx,PTSW1               ; Previous maximum points won
PT20:    MOV     ebx,offset PTSW1
;        CMP     al,(bx)                ; Compare to current value
         CMP     al,byte ptr [ebx]
;        JR      C,rel011               ; Jump if greater than
         JC      rel011
;        LD      al,(bx)                ; Load previous max value
         MOV     al,byte ptr [ebx]
;        LD      (bx),dl                ; Store new value as max won
         MOV     byte ptr [ebx],dl
;rel011: LD      bx,PTSW2               ; Previous 2nd max points won
rel011:  MOV     ebx,offset PTSW2
;        CMP     al,(bx)                ; Compare to current value
         CMP     al,byte ptr [ebx]
;        JR      C,PT23                 ; Jump if greater than
         JC      PT23
;        LD      (bx),al                ; Store as new 2nd max lost
         MOV     byte ptr [ebx],al
;PT23:   LD      bx,P1                  ; Get piece
PT23:    MOV     ebx,offset P1
;        BIT     7,(bx)                 ; Test color
         TEST    byte ptr [ebx],80h
;        LD      al,dh                  ; Value of piece
         MOV     al,dh
;        JR      Z,rel012               ; Jump if white
         JZ      rel012
;        NEG                            ; Negate for black
         NEG     al
;rel012: LD      bx,MTRL                ; Get addrs of material total
rel012:  MOV     ebx,offset MTRL
;        ADD     al,(bx)                ; Add new value
         ADD     al,byte ptr [ebx]
;        LD      (bx),al                ; Store
         MOV     byte ptr [ebx],al
;PT25:   LD      al,(M3)                ; Get current board position
PT25:    MOV     al,byte ptr [M3]
;        INC     al                     ; Increment
         INC     al
;        CMP     al,99                  ; At end of board ?
         CMP     al,99
;        JMP     NZ,PT5                 ; No - jump
         JNZ     PT5
;        LD      al,(PTSCK)             ; Moving piece lost flag
         MOV     al,byte ptr [PTSCK]
;        AND     al,al                  ; Was it lost ?
         AND     al,al
;        JR      Z,PT25A                ; No - jump
         JZ      PT25A
;        LD      al,(PTSW2)             ; 2nd max points won
         MOV     al,byte ptr [PTSW2]
;        LD      (PTSW1),al             ; Store as max points won
         MOV     byte ptr [PTSW1],al
;        XOR     al,al                  ; Zero out 2nd max points won
         XOR     al,al
;        LD      (PTSW2),al
         MOV     byte ptr [PTSW2],al
;PT25A:  LD      al,(PTSL)              ; Get max points lost
PT25A:   MOV     al,byte ptr [PTSL]
;        AND     al,al                  ; Is it zero ?
         AND     al,al
;        JR      Z,rel013               ; Yes - jump
         JZ      rel013
;        DEC     al                     ; Decrement it
         DEC     al
;rel013: LD      ch,al                  ; Save it
rel013:  MOV     ch,al
;        LD      al,(PTSW1)             ; Max,points won
         MOV     al,byte ptr [PTSW1]
;        AND     al,al                  ; Is it zero ?
         AND     al,al
;        JR      Z,rel014               ; Yes - jump
         JZ      rel014
;        LD      al,(PTSW2)             ; 2nd max points won
         MOV     al,byte ptr [PTSW2]
;        AND     al,al                  ; Is it zero ?
         AND     al,al
;        JR      Z,rel014               ; Yes - jump
         JZ      rel014
;        DEC     al                     ; Decrement it
         DEC     al
;        SRL     al                     ; Divide it by 2
         SHR     al,1
;        SUB     al,ch                  ; Subtract points lost
         SUB     al,ch
;rel014: LD      bx,COLOR               ; Color of side just moved ???
rel014:  MOV     ebx,offset COLOR
;        BIT     7,(bx)                 ; Is it white ?
         TEST    byte ptr [ebx],80h
;        JR      Z,rel015               ; Yes - jump
         JZ      rel015
;        NEG                            ; Negate for black
         NEG     al
;rel015: LD      bx,MTRL                ; Net material on board
rel015:  MOV     ebx,offset MTRL
;        ADD     al,(bx)                ; Add exchange adjustments
         ADD     al,byte ptr [ebx]
;        LD      bx,MV0                 ; Material at ply 0
         MOV     ebx,offset MV0
;X p52
;        SUB     al,(bx)                ; Subtract from current
         SUB     al,byte ptr [ebx]
;        LD      ch,al                  ; Save
         MOV     ch,al
;        LD      al,30                  ; Load material limit
         MOV     al,30
;        CALL    LIMIT                  ; Limit to plus or minus value
         CALL    LIMIT
;        LD      dl,al                  ; Save limited value
         MOV     dl,al
;        LD      al,(BRDC)              ; Get board control points
         MOV     al,byte ptr [BRDC]
;        LD      bx,BC0                 ; Board control at ply zero
         MOV     ebx,offset BC0
;        SUB     al,(bx)                ; Get difference
         SUB     al,byte ptr [ebx]
;        LD      ch,al                  ; Save
         MOV     ch,al
;        LD      al,(PTSCK)             ; Moving piece lost flag
         MOV     al,byte ptr [PTSCK]
;        AND     al,al                  ; Is it zero ?
         AND     al,al
;        JR      Z,rel026               ; Yes - jump
         JZ      rel026
;        LD      ch,0                   ; Zero board control points
         MOV     ch,0
;rel026: LD      al,6                   ; Load board control limit
rel026:  MOV     al,6
;        CALL    LIMIT                  ; Limit to plus or minus value
         CALL    LIMIT
;        LD      dh,al                  ; Save limited value
         MOV     dh,al
;        LD      al,dl                  ; Get material points
         MOV     al,dl
;        ADD     al,al                  ; Multiply by 4
         ADD     al,al
;        ADD     al,al
         ADD     al,al
;        ADD     al,dh                  ; Add board control
         ADD     al,dh
;        LD      bx,COLOR               ; Color of side just moved
         MOV     ebx,offset COLOR
;        BIT     7,(bx)                 ; Is it white ?
         TEST    byte ptr [ebx],80h
;        JR      NZ,rel016              ; No - jump
         JNZ     rel016
;        NEG                            ; Negate for white
         NEG     al
;rel016: ADD     al,80H                 ; Rescale score (neutral = 80H
rel016:  ADD     al,80H
;        LD      (VALM),al              ; Save score
         MOV     byte ptr [VALM],al
;        LD      si,(MLPTRJ)            ; Load move list pointer
         MOV     esi,[MLPTRJ]
;        LD      (si+MLVAL),al          ; Save score in move list
         MOV     byte ptr [esi+MLVAL],al
;        RET                            ; Return
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
;LIMIT:  BIT     7,ch                   ; Is value negative ?
LIMIT:   TEST    ch,80h
;        JMP     Z,LIM10                ; No - jump
         JZ      LIM10
;        NEG                            ; Make positive
         NEG     al
;        CMP     al,ch                  ; Compare to limit
         CMP     al,ch
;        RET     NC                     ; Return if outside limit
         JC      skip19
         RET
skip19:
;        LD      al,ch                  ; Output value as is
         MOV     al,ch
;        RET                            ; Return
         RET
;LIM10:  CMP     al,ch                  ; Compare to limit
LIM10:   CMP     al,ch
;        RET     C                      ; Return if outside limit
         JNC     skip20
         RET
skip20:
;        LD      al,ch                  ; Output value as is
         MOV     al,ch
;        RET                            ; Return
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
;MOVE:   LD      bx,(MLPTRJ)            ; Load move list pointer
MOVE:    MOV     ebx,[MLPTRJ]
;        INC     bx                     ; Increment past link bytes
;        INC     bx
         LEA     ebx,[ebx+4]
;MV1:    LD      al,(bx)                ; "From" position
MV1:     MOV     al,byte ptr [ebx]
;        LD      (M1),al                ; Save
         MOV     byte ptr [M1],al
;        INC     bx                     ; Increment pointer
         LEA     ebx,[ebx+1]
;        LD      al,(bx)                ; "To" position
         MOV     al,byte ptr [ebx]
;        LD      (M2),al                ; Save
         MOV     byte ptr [M2],al
;        INC     bx                     ; Increment pointer
         LEA     ebx,[ebx+1]
;        LD      dh,(bx)                ; Get captured piece/flags
         MOV     dh,byte ptr [ebx]
;        LD      si,(M1)                ; Load "from" pos board index
         MOV     esi,[M1]
;        LD      dl,(si+BOARD)          ; Get piece moved
         MOV     dl,byte ptr [esi+BOARD]
;        BIT     5,dh                   ; Test Pawn promotion flag
         TEST    dh,20h
;        JR      NZ,MV15                ; Jump if set
         JNZ     MV15
;        LD      al,dl                  ; Piece moved
         MOV     al,dl
;        AND     al,7                   ; Clear flag bits
         AND     al,7
;        CMP     al,QUEEN               ; Is it a queen ?
         CMP     al,QUEEN
;        JR      Z,MV20                 ; Yes - jump
         JZ      MV20
;        CMP     al,KING                ; Is it a king ?
         CMP     al,KING
;        JR      Z,MV30                 ; Yes - jump
         JZ      MV30
;MV5:    LD      di,(M2)                ; Load "to" pos board index
MV5:     MOV     edi,[M2]
;        SET     3,dl                   ; Set piece moved flag
         LAHF
         OR      dl,8
         SAHF
;        LD      (di+BOARD),dl          ; Insert piece at new position
         MOV     byte ptr [edi+BOARD],dl
;        LD      (si+BOARD),0           ; Empty previous position
         MOV     byte ptr [esi+BOARD],0
;        BIT     6,dh                   ; Double move ?
         TEST    dh,40h
;        JR      NZ,MV40                ; Yes - jump
         JNZ     MV40
;        LD      al,dh                  ; Get captured piece, if any
         MOV     al,dh
;        AND     al,7
         AND     al,7
;        CMP     al,QUEEN               ; Was it a queen ?
         CMP     al,QUEEN
;        RET     NZ                     ; No - return
         JZ      skip21
         RET
skip21:
;        LD      bx,POSQ                ; Addr of saved Queen position
         MOV     ebx,offset POSQ
;        BIT     7,dh                   ; Is Queen white ?
         TEST    dh,80h
;        JR      Z,MV10                 ; Yes - jump
         JZ      MV10
;        INC     bx                     ; Increment to black Queen pos
         LEA     ebx,[ebx+1]
;X p55
;MV10:   XOR     al,al                  ; Set saved position to zero
MV10:    XOR     al,al
;        LD      (bx),al
         MOV     byte ptr [ebx],al
;        RET                            ; Return
         RET
;MV15:   SET     2,dl                   ; Change Pawn to a Queen
MV15:    LAHF
         OR      dl,4
         SAHF
;        JMP     MV5                    ; Jump
         JMP     MV5
;MV20:   LD      bx,POSQ                ; Addr of saved Queen position
MV20:    MOV     ebx,offset POSQ
;MV21:   BIT     7,dl                   ; Is Queen white ?
MV21:    TEST    dl,80h
;        JR      Z,MV22                 ; Yes - jump
         JZ      MV22
;        INC     bx
         LEA     ebx,[ebx+1]
;MV22:   LD      al,(M2)                ; Get rnewnQueenbpositionen pos
MV22:    MOV     al,byte ptr [M2]
;        LD      (bx),al                ; Save
         MOV     byte ptr [ebx],al
;        JMP     MV5                    ; Jump
         JMP     MV5
;MV30:   LD      bx,POSK                ; Get saved King position
MV30:    MOV     ebx,offset POSK
;        BIT     6,dh                   ; Castling ?
         TEST    dh,40h
;        JR      Z,MV21                 ; No - jump
         JZ      MV21
;        SET     4,dl                   ; Set King castled flag
         LAHF
         OR      dl,10h
         SAHF
;        JMP     MV21                   ; Jump
         JMP     MV21
;MV40:   LD      bx,(MLPTRJ)            ; Get move list pointer
MV40:    MOV     ebx,[MLPTRJ]
;        LD      dx,8                   ; Increment to next move
         MOV     edx,8
;        ADD     bx,dx
         LEA     ebx,[ebx+edx]
;        JMP     MV1                    ; Jump (2nd part of dbl move)
         JMP     MV1

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
;UNMOVE: LD      bx,(MLPTRJ)            ; Load move list pointer
UNMOVE:  MOV     ebx,[MLPTRJ]
;        INC     bx                     ; Increment past link bytes
;        INC     bx
         LEA     ebx,[ebx+4]
;UM1:    LD      al,(bx)                ; Get "from" position
UM1:     MOV     al,byte ptr [ebx]
;        LD      (M1),al                ; Save
         MOV     byte ptr [M1],al
;        INC     bx                     ; Increment pointer
         LEA     ebx,[ebx+1]
;        LD      al,(bx)                ; Get "to" position
         MOV     al,byte ptr [ebx]
;        LD      (M2),al                ; Save
         MOV     byte ptr [M2],al
;        INC     bx                     ; Increment pointer
         LEA     ebx,[ebx+1]
;        LD      dh,(bx)                ; Get captured piece/flags
         MOV     dh,byte ptr [ebx]
;        LD      si,(M2)                ; Load "to" pos board index
         MOV     esi,[M2]
;        LD      dl,(si+BOARD)          ; Get piece moved
         MOV     dl,byte ptr [esi+BOARD]
;        BIT     5,dh                   ; Was it a Pawn promotion ?
         TEST    dh,20h
;        JR      NZ,UM15                ; Yes - jump
         JNZ     UM15
;        LD      al,dl                  ; Get piece moved
         MOV     al,dl
;        AND     al,7                   ; Clear flag bits
         AND     al,7
;        CMP     al,QUEEN               ; Was it a Queen ?
         CMP     al,QUEEN
;        JR      Z,UM20                 ; Yes - jump
         JZ      UM20
;        CMP     al,KING                ; Was it a King ?
         CMP     al,KING
;        JR      Z,UM30                 ; Yes - jump
         JZ      UM30
;UM5:    BIT     4,dh                   ; Is this 1st move for piece ?
UM5:     TEST    dh,10h
;        JR      NZ,UM16                ; Yes - jump
         JNZ     UM16
;UM6:    LD      di,(M1)                ; Load "from" pos board index
UM6:     MOV     edi,[M1]
;        LD      (di+BOARD),dl          ; Return to previous board pos
         MOV     byte ptr [edi+BOARD],dl
;        LD      al,dh                  ; Get captured piece, if any
         MOV     al,dh
;        AND     al,8FH                 ; Clear flags
         AND     al,8FH
;        LD      (si+BOARD),al          ; Return to board
         MOV     byte ptr [esi+BOARD],al
;        BIT     6,dh                   ; Was it a double move ?
         TEST    dh,40h
;        JR      NZ,UM40                ; Yes - jump
         JNZ     UM40
;        LD      al,dh                  ; Get captured piece, if any
         MOV     al,dh
;        AND     al,7                   ; Clear flag bits
         AND     al,7
;        CMP     al,QUEEN               ; Was it a Queen ?
         CMP     al,QUEEN
;        RET     NZ                     ; No - return
         JZ      skip22
         RET
skip22:
;X p57
;        LD      bx,POSQ                ; Address of saved Queen pos
         MOV     ebx,offset POSQ
;        BIT     7,dh                   ; Is Queen white ?
         TEST    dh,80h
;        JR      Z,UM10                 ; Yes - jump
         JZ      UM10
;        INC     bx                     ; Increment to black Queen pos
         LEA     ebx,[ebx+1]
;UM10:   LD      al,(M2)                ; Queen's previous position
UM10:    MOV     al,byte ptr [M2]
;        LD      (bx),al                ; Save
         MOV     byte ptr [ebx],al
;        RET                            ; Return
         RET
;UM15:   RES     2,dl                   ; Restore Queen to Pawn
UM15:    LAHF
         AND     dl,0fbh
         SAHF
;        JMP     UM5                    ; Jump
         JMP     UM5
;UM16:   RES     3,dl                   ; Clear piece moved flag
UM16:    LAHF
         AND     dl,0f7h
         SAHF
;        JMP     UM6                    ; Jump
         JMP     UM6
;UM20:   LD      bx,POSQ                ; Addr of saved Queen position
UM20:    MOV     ebx,offset POSQ
;UM21:   BIT     7,dl                   ; Is Queen white ?
UM21:    TEST    dl,80h
;        JR      Z,UM22                 ; Yes - jump
         JZ      UM22
;        INC     bx                     ; Increment to black Queen pos
         LEA     ebx,[ebx+1]
;UM22:   LD      al,(M1)                ; Get previous position
UM22:    MOV     al,byte ptr [M1]
;        LD      (bx),al                ; Save
         MOV     byte ptr [ebx],al
;        JMP     UM5                    ; Jump
         JMP     UM5
;UM30:   LD      bx,POSK                ; Address of saved King pos
UM30:    MOV     ebx,offset POSK
;        BIT     6,dh                   ; Was it a castle ?
         TEST    dh,40h
;        JR      Z,UM21                 ; No - jump
         JZ      UM21
;        RES     4,dl                   ; Clear castled flag
         LAHF
         AND     dl,0efh
         SAHF
;        JMP     UM21                   ; Jump
         JMP     UM21
;UM40:   LD      bx,(MLPTRJ)            ; Load move list pointer
UM40:    MOV     ebx,[MLPTRJ]
;        LD      dx,8                   ; Increment to next move
         MOV     edx,8
;        ADD     bx,dx
         LEA     ebx,[ebx+edx]
;        JMP     UM1                    ; Jump (2nd part of dbl move)
         JMP     UM1

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
;SORTM:  LD      cx,(MLPTRI)            ; Move list begin pointer
SORTM:   MOV     ecx,[MLPTRI]
;        LD      dx,0                   ; Initialize working pointers
         MOV     edx,0
;SR5:    LD      bh,ch
;        LD      bl,cl
SR5:     MOV     ebx,ecx
;        LD      cl,(bx)                ; Link to next move
;        INC     bx
;        LD      ch,(bx)
         MOV     ecx,dword ptr [ebx]
;        LD      (bx),dh                ; Store to link in list
;        DEC     bx
;        LD      (bx),dl
         MOV     dword ptr [ebx],edx
;        XOR     al,al                  ; End of list ?
;        CMP     al,ch
         CMP     ecx,0
;        RET     Z                      ; Yes - return
         JNZ     skip23
         RET
skip23:
;SR10:   LD      (MLPTRJ),cx            ; Save list pointer
SR10:    MOV     [MLPTRJ],ecx
;        CALL    EVAL                   ; Evaluate move
         CALL    EVAL
;        LD      bx,(MLPTRI)            ; Begining of move list
         MOV     ebx,[MLPTRI]
;        LD      cx,(MLPTRJ)            ; Restore list pointer
         MOV     ecx,[MLPTRJ]
;SR15:   LD      dl,(bx)                ; Next move for compare
;        INC     bx
;        LD      dh,(bx)
SR15:    MOV     edx,dword ptr [ebx]
;        XOR     al,al                  ; At end of list ?
;        CMP     al,dh
         CMP     edx,0
;        JR      Z,SR25                 ; Yes - jump
         JZ      SR25
;        PUSH    dx                     ; Transfer move pointer
         PUSH edx
;        POP     si
         POP esi
;        LD      al,(VALM)              ; Get new move value
         MOV     al,byte ptr [VALM]
;        CMP     al,(si+MLVAL)          ; Less than list value ?
         CMP     al,byte ptr [esi+MLVAL]
;        JR      NC,SR30                ; No - jump
         JNC     SR30
;SR25:   LD      (bx),ch                ; Link new move into list
;        DEC     bx
;        LD      (bx),cl
SR25:    MOV     dword ptr [ebx],ecx
;        JMP     SR5                    ; Jump
         JMP     SR5
;SR30:   EX      dx,bx                  ; Swap pointers
SR30:    XCHG    ebx,edx
;        JMP     SR15                   ; Jump
         JMP     SR15

;X p59
;**********************************************************
; EVALUATION ROUTINE
;**********************************************************
; FUNCTION: --  To evaluate a given move in the move list.
;               It first makes the move on the board, then ii
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
;EVAL:   CALL    MOVE                   ; Make move on the board array
EVAL:    CALL    MOVE
;        CALL    INCHK                  ; Determine if move is legal
         CALL    INCHK
;        AND     al,al                  ; Legal move ?
         AND     al,al
;        JR      Z,EV5                  ; Yes - jump
         JZ      EV5
;        XOR     al,al                  ; Score of zero
         XOR     al,al
;        LD      (VALM),al              ; For illegal move
         MOV     byte ptr [VALM],al
;        JMP     EV10                   ; Jump
         JMP     EV10
;EV5:    CALL    PINFND                 ; Compile pinned list
EV5:     CALL    PINFND
;        CALL    POINTS                 ; Assign points to move
         CALL    POINTS
;EV10:   CALL    UNMOVE                 ; Restore board array
EV10:    CALL    UNMOVE
;        RET                            ; Return
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
;FNDMOV: LD      al,(MOVENO)            ; Currnet move number
FNDMOV:  MOV     al,byte ptr [MOVENO]
;        CMP     al,1                   ; First move ?
         CMP     al,1
;        CALL    Z,BOOK                 ; Yes - execute book opening
         JNZ     skip24
         CALL    BOOK
skip24:
;        XOR     al,al                  ; Initialize ply number to zer
         XOR     al,al
;        LD      (NPLY),al
         MOV     byte ptr [NPLY],al
;        LD      bx,0                   ; Initialize best move to zero
         MOV     ebx,0
;        LD      (BESTM),bx
         MOV     [BESTM],ebx
;        LD      bx,MLIST               ; Initialize ply list pointers
         MOV     ebx,offset MLIST
;        LD      (MLNXT),bx
         MOV     [MLNXT],ebx
;        LD      bx,PLYIX-2
         MOV     ebx,offset PLYIX-2
;        LD      (MLPTRI),bx
         MOV     [MLPTRI],ebx
;        LD      al,(KOLOR)             ; Initialize color
         MOV     al,byte ptr [KOLOR]
;        LD      (COLOR),al
         MOV     byte ptr [COLOR],al
;        LD      bx,SCORE               ; Initialize score index
         MOV     ebx,offset SCORE
;        LD      (SCRIX),bx
         MOV     [SCRIX],ebx
;        LD      al,(PLYMAX)            ; Get max ply number
         MOV     al,byte ptr [PLYMAX]
;        ADD     al,2                   ; Add 2
         ADD     al,2
;        LD      ch,al                  ; Save as counter
         MOV     ch,al
;        XOR     al,al                  ; Zero out score table
         XOR     al,al
;back05: LD      (bx),al
back05:  MOV     byte ptr [ebx],al
;        INC     bx
         LEA     ebx,[ebx+1]
;        DJNZ    back05
         LAHF
         DEC ch
         JNZ     back05
         SAHF
;        LD      (BC0),al               ; Zero ply 0 board control
         MOV     byte ptr [BC0],al
;        LD      (MV0),al               ; Zero ply 0 material
         MOV     byte ptr [MV0],al
;        CALL    PINFND                 ; Complie pin list
         CALL    PINFND
;        CALL    POINTS                 ; Evaluate board at ply 0
         CALL    POINTS
;        LD      al,(BRDC)              ; Get board control points
         MOV     al,byte ptr [BRDC]
;        LD      (BC0),al               ; Save
         MOV     byte ptr [BC0],al
;        LD      al,(MTRL)              ; Get material count
         MOV     al,byte ptr [MTRL]
;        LD      (MV0),al               ; Save
         MOV     byte ptr [MV0],al
;FM5:    LD      bx,NPLY                ; Address of ply counter
FM5:     MOV     ebx,offset NPLY
;        INC     (bx)                   ; Increment ply count
         INC     byte ptr [ebx]
;X p61
;        XOR     al,al                  ; Initialize mate flag
         XOR     al,al
;        LD      (MATEF),al
         MOV     byte ptr [MATEF],al
;        CALL    GENMOV                 ; Generate list of moves
         CALL    GENMOV
;        LD      al,(NPLY)              ; Current ply counter
         MOV     al,byte ptr [NPLY]
;        LD      bx,PLYMAX              ; Address of maximum ply number
         MOV     ebx,offset PLYMAX
;        CMP     al,(bx)                ; At max ply ?
         CMP     al,byte ptr [ebx]
;        CALL    C,SORTM                ; No - call sort
         JNC     skip25
         CALL    SORTM
skip25:
;        LD      bx,(MLPTRI)            ; Load ply index pointer
         MOV     ebx,[MLPTRI]
;        LD      (MLPTRJ),bx            ; Save as last move pointer
         MOV     [MLPTRJ],ebx
;FM15:   LD      bx,(MLPTRJ)            ; Load last move pointer
FM15:    MOV     ebx,[MLPTRJ]
;        LD      dl,(bx)                ; Get next move pointer
         MOV     dl,byte ptr [ebx]
;        INC     bx
         LEA     ebx,[ebx+1]
;        LD      dh,(bx)
         MOV     dh,byte ptr [ebx]
;        LD      al,dh
         MOV     al,dh
;        AND     al,al                  ; End of move list ?
         AND     al,al
;        JR      Z,FM25                 ; Yes - jump
         JZ      FM25
;        LD      (MLPTRJ),dx            ; Save current move pointer
         MOV     [MLPTRJ],edx
;        LD      bx,(MLPTRI)            ; Save in ply pointer list
         MOV     ebx,[MLPTRI]
;        LD      (bx),dl
         MOV     byte ptr [ebx],dl
;        INC     bx
         LEA     ebx,[ebx+1]
;        LD      (bx),dh
         MOV     byte ptr [ebx],dh
;        LD      al,(NPLY)              ; Current ply counter
         MOV     al,byte ptr [NPLY]
;        LD      bx,PLYMAX              ; Maximum ply number ?
         MOV     ebx,offset PLYMAX
;        CMP     al,(bx)                ; Compare
         CMP     al,byte ptr [ebx]
;        JR      C,FM18                 ; Jump if not max
         JC      FM18
;        CALL    MOVE                   ; Execute move on board array
         CALL    MOVE
;        CALL    INCHK                  ; Check for legal move
         CALL    INCHK
;        AND     al,al                  ; Is move legal
         AND     al,al
;        JR      Z,rel017               ; Yes - jump
         JZ      rel017
;        CALL    UNMOVE                 ; Restore board position
         CALL    UNMOVE
;        JMP     FM15                   ; Jump
         JMP     FM15
;rel017: LD      al,(NPLY)              ; Get ply counter
rel017:  MOV     al,byte ptr [NPLY]
;        LD      bx,PLYMAX              ; Max ply number
         MOV     ebx,offset PLYMAX
;        CMP     al,(bx)                ; Beyond max ply ?
         CMP     al,byte ptr [ebx]
;        JR      NZ,FM35                ; Yes - jump
         JNZ     FM35
;        LD      al,(COLOR)             ; Get current color
         MOV     al,byte ptr [COLOR]
;        XOR     al,80H                 ; Get opposite color
         XOR     al,80H
;        CALL    INCHK1                 ; Determine if King is in check
         CALL    INCHK1
;        AND     al,al                  ; In check ?
         AND     al,al
;        JR      Z,FM35                 ; No - jump
         JZ      FM35
;        JMP     FM19                   ; Jump (One more ply for check)
         JMP     FM19
;FM18:   LD      si,(MLPTRJ)            ; Load move pointer
FM18:    MOV     esi,[MLPTRJ]
;        LD      al,(si+MLVAL)          ; Get move score
         MOV     al,byte ptr [esi+MLVAL]
;        AND     al,al                  ; Is it zero (illegal move) ?
         AND     al,al
;        JR      Z,FM15                 ; Yes - jump
         JZ      FM15
;        CALL    MOVE                   ; Execute move on board array
         CALL    MOVE
;FM19:   LD      bx,COLOR               ; Toggle color
FM19:    MOV     ebx,offset COLOR
;        LD      al,80H
         MOV     al,80H
;        XOR     al,(bx)
         XOR     al,byte ptr [ebx]
;        LD      (bx),al                ; Save new color
         MOV     byte ptr [ebx],al
;X p62
;        BIT     7,al                   ; Is it white ?
         TEST    al,80h
;        JR      NZ,rel018              ; No - jump
         JNZ     rel018
;        LD      bx,MOVENO              ; Increment move number
         MOV     ebx,offset MOVENO
;        INC     (bx)
         INC     byte ptr [ebx]
;rel018: LD      bx,(SCRIX)             ; Load score table pointer
rel018:  MOV     ebx,[SCRIX]
;        LD      al,(bx)                ; Get score two plys above
         MOV     al,byte ptr [ebx]
;        INC     bx                     ; Increment to current ply
         LEA     ebx,[ebx+1]
;        INC     bx
         LEA     ebx,[ebx+1]
;        LD      (bx),al                ; Save score as initial value
         MOV     byte ptr [ebx],al
;        DEC     bx                     ; Decrement pointer
         LEA     ebx,[ebx-1]
;        LD      (SCRIX),bx             ; Save it
         MOV     [SCRIX],ebx
;        JMP     FM5                    ; Jump
         JMP     FM5
;FM25:   LD      al,(MATEF)             ; Get mate flag
FM25:    MOV     al,byte ptr [MATEF]
;        AND     al,al                  ; Checkmate or stalemate ?
         AND     al,al
;        JR      NZ,FM30                ; No - jump
         JNZ     FM30
;        LD      al,(CKFLG)             ; Get check flag
         MOV     al,byte ptr [CKFLG]
;        AND     al,al                  ; Was King in check ?
         AND     al,al
;        LD      al,80H                 ; Pre-set stalemate score
         MOV     al,80H
;        JR      Z,FM36                 ; No - jump (stalemate)
         JZ      FM36
;        LD      al,(MOVENO)            ; Get move number
         MOV     al,byte ptr [MOVENO]
;        LD      (PMATE),al             ; Save
         MOV     byte ptr [PMATE],al
;        LD      al,0FFH                ; Pre-set checkmate score
         MOV     al,0FFH
;        JMP     FM36                   ; Jump
         JMP     FM36
;FM30:   LD      al,(NPLY)              ; Get ply counter
FM30:    MOV     al,byte ptr [NPLY]
;        CMP     al,1                   ; At top of tree ?
         CMP     al,1
;        RET     Z                      ; Yes - return
         JNZ     skip26
         RET
skip26:
;        CALL    ASCEND                 ; Ascend one ply in tree
         CALL    ASCEND
;        LD      bx,(SCRIX)             ; Load score table pointer
         MOV     ebx,[SCRIX]
;        INC     bx                     ; Increment to current ply
         LEA     ebx,[ebx+1]
;        INC     bx
         LEA     ebx,[ebx+1]
;        LD      al,(bx)                ; Get score
         MOV     al,byte ptr [ebx]
;        DEC     bx                     ; Restore pointer
         LEA     ebx,[ebx-1]
;        DEC     bx
         LEA     ebx,[ebx-1]
;        JMP     FM37                   ; Jump
         JMP     FM37
;FM35:   CALL    PINFND                 ; Compile pin list
FM35:    CALL    PINFND
;        CALL    POINTS                 ; Evaluate move
         CALL    POINTS
;        CALL    UNMOVE                 ; Restore board position
         CALL    UNMOVE
;        LD      al,(VALM)              ; Get value of move
         MOV     al,byte ptr [VALM]
;FM36:   LD      bx,MATEF               ; Set mate flag
FM36:    MOV     ebx,offset MATEF
;        SET     0,(bx)
         LAHF
         OR      byte ptr [ebx],1
         SAHF
;        LD      bx,(SCRIX)             ; Load score table pointer
         MOV     ebx,[SCRIX]
;FM37:   CMP     al,(bx)                ; Compare to score 2 ply above
FM37:    CMP     al,byte ptr [ebx]
;        JR      C,FM40                 ; Jump if less
         JC      FM40
;        JR      Z,FM40                 ; Jump if equal
         JZ      FM40
;        NEG                            ; Negate score
         NEG     al
;        INC     bx                     ; Incr score table pointer
         LEA     ebx,[ebx+1]
;        CMP     al,(bx)                ; Compare to score 1 ply above
         CMP     al,byte ptr [ebx]
;        JMP     C,FM15                 ; Jump if less than
         JC      FM15
;        JMP     Z,FM15                 ; Jump if equal
         JZ      FM15
;X p63
;        LD      (bx),al                ; Save as new score 1 ply above
         MOV     byte ptr [ebx],al
;        LD      al,(NPLY)              ; Get current ply counter
         MOV     al,byte ptr [NPLY]
;        CMP     al,1                   ; At top of tree ?
         CMP     al,1
;        JMP     NZ,FM15                ; No - jump
         JNZ     FM15
;        LD      bx,(MLPTRJ)            ; Load current move pointer
         MOV     ebx,[MLPTRJ]
;        LD      (BESTM),bx             ; Save as best move.pointer
         MOV     [BESTM],ebx
;        LD      al,(SCORE+1)           ; Get best move score
         MOV     al,byte ptr [SCORE+1]
;        CMP     al,0FFH                ; Was it a checkmate ?
         CMP     al,0FFH
;        JMP     NZ,FM15                ; No - jump
         JNZ     FM15
;        LD      bx,PLYMAX              ; Get maximum ply number
         MOV     ebx,offset PLYMAX
;        DEC     (bx)                   ; Subtract 2
         DEC     byte ptr [ebx]
;        DEC     (bx)
         DEC     byte ptr [ebx]
;        LD      al,(KOLOR)             ; Get computer's color
         MOV     al,byte ptr [KOLOR]
;        BIT     7,al                   ; Is it white ?
         TEST    al,80h
;        RET     Z                      ; Yes - return
         JNZ     skip27
         RET
skip27:
;        LD      bx,PMATE               ; Checkmate move number
         MOV     ebx,offset PMATE
;        DEC     (bx)                   ; Decrement
         DEC     byte ptr [ebx]
;        RET                            ; Return
         RET
;FM40:   CALL    ASCEND                 ; Ascend one ply in tree
FM40:    CALL    ASCEND
;        JMP     FM15                   ; Jump
         JMP     FM15

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
;ASCEND: LD      bx,COLOR               ; Toggle color
ASCEND:  MOV     ebx,offset COLOR
;        LD      al,80H
         MOV     al,80H
;        XOR     al,(bx)
         XOR     al,byte ptr [ebx]
;        LD      (bx),al                ; Save new color
         MOV     byte ptr [ebx],al
;        BIT     7,al                   ; Is it white ?
         TEST    al,80h
;        JR      Z,rel019               ; Yes - jump
         JZ      rel019
;        LD      bx,MOVENO              ; Decrement move number
         MOV     ebx,offset MOVENO
;        DEC     (bx)
         DEC     byte ptr [ebx]
;rel019: LD      bx,(SCRIX)             ; Load score table index
rel019:  MOV     ebx,[SCRIX]
;        DEC     bx                     ; Decrement
         LEA     ebx,[ebx-1]
;        LD      (SCRIX),bx             ; Save
         MOV     [SCRIX],ebx
;        LD      bx,NPLY                ; Decrement ply counter
         MOV     ebx,offset NPLY
;        DEC     (bx)
         DEC     byte ptr [ebx]
;        LD      bx,(MLPTRI)            ; Load ply list pointer
         MOV     ebx,[MLPTRI]
;        DEC     bx                     ; Load pointer to move list to
         LEA     ebx,[ebx-1]
;        LD      dh,(bx)
         MOV     dh,byte ptr [ebx]
;        DEC     bx
         LEA     ebx,[ebx-1]
;        LD      dl,(bx)
         MOV     dl,byte ptr [ebx]
;        LD      (MLNXT),dx             ; Update move list avail ptr
         MOV     [MLNXT],edx
;        DEC     bx                     ; Get ptr to next move to undo
         LEA     ebx,[ebx-1]
;        LD      dh,(bx)
         MOV     dh,byte ptr [ebx]
;        DEC     bx
         LEA     ebx,[ebx-1]
;        LD      dl,(bx)
         MOV     dl,byte ptr [ebx]
;        LD      (MLPTRI),bx            ; Save new ply list pointer
         MOV     [MLPTRI],ebx
;        LD      (MLPTRJ),dx            ; Save next move pointer
         MOV     [MLPTRJ],edx
;        CALL    UNMOVE                 ; Restore board to previous pl
         CALL    UNMOVE
;        RET                            ; Return
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
;BOOK:   POP     af                     ; Abort return to FNDMOV
BOOK:    POP eax
         sahf
;        LD      bx,SCORE+1             ; Zero out score
         MOV     ebx,offset SCORE+1
;        LD      (bx),0                 ; Zero out score table
         MOV     byte ptr [ebx],0
;        LD      bx,BMOVES-2            ; Init best move ptr to book
         MOV     ebx,offset BMOVES-2
;        LD      (BESTM),bx
         MOV     [BESTM],ebx
;        LD      bx,BESTM               ; Initialize address of pointer
         MOV     ebx,offset BESTM
;        LD      al,(KOLOR)             ; Get computer's color
         MOV     al,byte ptr [KOLOR]
;        AND     al,al                  ; Is it white ?
         AND     al,al
;        JR      NZ,BM5                 ; No - jump
         JNZ     BM5
;        LDAR                           ; Load refresh reg (random no)
         Z80_LDAR
;        BIT     0,al                   ; Test random bit
         TEST    al,1
;        RET     Z                      ; Return if zero (P-K4)
         JNZ     skip28
         RET
skip28:
;        INC     (bx)                   ; P-Q4
         INC     byte ptr [ebx]
;        INC     (bx)
         INC     byte ptr [ebx]
;        INC     (bx)
         INC     byte ptr [ebx]
;        RET                            ; Return
         RET
;BM5:    INC     (bx)                   ; Increment to black moves
BM5:     INC     byte ptr [ebx]
;        INC     (bx)
         INC     byte ptr [ebx]
;        INC     (bx)
         INC     byte ptr [ebx]
;        INC     (bx)
         INC     byte ptr [ebx]
;        INC     (bx)
         INC     byte ptr [ebx]
;        INC     (bx)
         INC     byte ptr [ebx]
;        LD      si,(MLPTRJ)            ; Pointer to opponents 1st move
         MOV     esi,[MLPTRJ]
;        LD      al,(si+MLFRP)          ; Get "from" position
         MOV     al,byte ptr [esi+MLFRP]
;        CMP     al,22                  ; Is it a Queen Knight move ?
         CMP     al,22
;        JR      Z,BM9                  ; Yes - Jump
         JZ      BM9
;        CMP     al,27                  ; Is it a King Knight move ?
         CMP     al,27
;        JR      Z,BM9                  ; Yes - jump
         JZ      BM9
;        CMP     al,34                  ; Is it a Queen Pawn ?
         CMP     al,34
;        JR      Z,BM9                  ; Yes - jump
         JZ      BM9
;        RET     C                      ; If Queen side Pawn opening -
         JNC     skip29
         RET
skip29:
                                ; return (P-K4)
;        CMP     al,35                  ; Is it a King Pawn ?
         CMP     al,35
;        RET     Z                      ; Yes - return (P-K4)
         JNZ     skip30
         RET
skip30:
;BM9:    INC     (bx)                   ; (P-Q4)
BM9:     INC     byte ptr [ebx]
;        INC     (bx)
         INC     byte ptr [ebx]
;        INC     (bx)
         INC     byte ptr [ebx]
;        RET                            ; Return to CPTRMV
         RET

        
;X p66
;**********************************************************
; GRAPHICS DATA BASE
;**********************************************************
; DESCRIPTION:  The Graphics Data Base contains the
;               necessary stored data to produce the piece
;               on the board. Only the center 4 x 4 blocks are
;               stored and only for a Black Piece on a White
;               square. A White piece on a black square is
;               produced by complementing each block, and a
;               piece on its own color square is produced
;               by moving in a kernel of 6 blocks.
;**********************************************************

;X p67
;**********************************************************
; STANDARD MESSAGES
;**********************************************************
;X appended "$" for CP/M C.PRINTSTR call
;X      .ASCII  [^H83]  ; Part of TITLE 3 - Underlines
;X      .ASCII  [^H83]
;X      .ASCII  [^H83]
;X      .ASCII  [^H83]
;X      .ASCII  [^H83]
;X      .ASCII  [^H83]
;X      .ASCII  " "
;X      .ASCII  [^H83]
;X      .ASCII  [^H83]
;X      .ASCII  [^H83]
;X      .ASCII  [^H83]
;X      .ASCII  [^H83]
;X      .ASCII  [^H83]
;X      .ASCII  " " "$"

;X p68
;**********************************************************
; VARIABLES
;**********************************************************
                        ; corner of the square on the board

;**********************************************************
; MACRO DEFINITIONS
;**********************************************************
; All input/output to SARGON is handled in the form of
; macro calls to simplify conversion to alternate systems.
; All of the input/output macros conform to the Jove monitor
; of the Jupiter III computer.
;**********************************************************
;X *** OUTPUT <CR><LF> ***
;X *** CLEAR SCREEN ***
;X *** PRINT ANY LINE (NAME, LENGTH) ***
;X *** PRINT ANY BLOCK (NAME, LENGTH) ***
;X *** EXIT TO MONITOR ***
;X *** Single char input ***
;X *** Single char echo ***

;X p69
;**********************************************************
; MAIN PROGRAM DRIVER
;**********************************************************
; FUNCTION: --  To coordinate the game moves.
;
; CALLED BY: -- None
;
; CALLS: --     INTERR
;               INITBD
;               DSPBRD
;               CPTRMV
;               PLYRMV
;               TBCPCL
;               PGIFND
;
; MACRO CALLS:  CLRSCR
;               CARRET
;               PRTLIN
;               PRTBLK
;
; ARGUMENTS:    None
;**********************************************************
;X p70

;X p71
;*****************************************************************
; INTERROGATION FOR PLY & COLOR
;*****************************************************************
; FUNCTION: --  To query the player for his choice of ply
;               depth and color.
;
; CALLED BY: -- DRIVER
;
; CALLS: --     CHARTR
;
; MACRO CALLS:  PRTLIN
;               CARRET
;
; ARGUMENTS: -- None
;X p72
;*****************************************************************

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
;CPTRMV: CALL    FNDMOV                 ; Select best move
CPTRMV:  CALL    FNDMOV
;        LD      bx,(BESTM)             ; Move list pointer variable
         MOV     ebx,[BESTM]
;        LD      (MLPTRJ),bx            ; Pointer to move data
         MOV     [MLPTRJ],ebx
;        LD      al,(SCORE+1)           ; To check for mates
         MOV     al,byte ptr [SCORE+1]
;        CMP     al,1                   ; Mate against computer ?
         CMP     al,1
;        JR      NZ,CP0C                ; No - jump
         JNZ     CP0C
;        LD      cl,1                   ; Computer mate flag
         MOV     cl,1
;        CALL    FCDMAT                 ; Full checkmate ?
         CALL    FCDMAT
;CP0C:   CALL    MOVE                   ; Produce move on board array
CP0C:    CALL    MOVE
;        CALL    EXECMV                 ; Make move on graphics board
         CALL    EXECMV
                                ; and return info about it
;        LD      al,ch                  ; Special move flags
         MOV     al,ch
;        AND     al,al                  ; Special ?
         AND     al,al
;        JR      NZ,CP10                ; Yes - jump
         JNZ     CP10
;        LD      dh,dl                  ; "To" position of the move
         MOV     dh,dl
;        CALL    BITASN                 ; Convert to Ascii
         CALL    BITASN
;        LD      (MVEMSG_2),bx          ;todo MVEMSG+3        ; Put in move message
         MOV     [MVEMSG_2],ebx
;        LD      dh,cl                  ; "From" position of the move
         MOV     dh,cl
;        CALL    BITASN                 ; Convert to Ascii
         CALL    BITASN
;        LD      (MVEMSG),bx            ; Put in move message
         MOV     [MVEMSG],ebx
;        PRTBLK  MVEMSG,5               ; Output text of move
         PRTBLK  MVEMSG,5
;        JR      CP1C                   ; Jump
         JMP     CP1C
;CP10:   BIT     1,ch                   ; King side castle ?
CP10:    TEST    ch,2
;        JR      Z,rel020               ; No - jump
         JZ      rel020
;        PRTBLK  O.O,5                  ; Output "O-O"
         PRTBLK  O.O,5
;        JR      CP1C                   ; Jump
         JMP     CP1C
;rel020: BIT     2,ch                   ; Queen side castle ?
rel020:  TEST    ch,4
;        JR      Z,rel021               ; No - jump
         JZ      rel021
;X p74
;        PRTBLK  O.O.O,5                ;  Output "O-O-O"
         PRTBLK  O.O.O,5
;        JR      CP1C                   ; Jump
         JMP     CP1C
;rel021: PRTBLK  P.PEP,5                ; Output "PxPep" - En passant
rel021:  PRTBLK  P.PEP,5
;CP1C:   LD      al,(COLOR)             ; Should computer call check ?
CP1C:    MOV     al,byte ptr [COLOR]
;        LD      ch,al
         MOV     ch,al
;        XOR     al,80H                 ; Toggle color
         XOR     al,80H
;        LD      (COLOR),al
         MOV     byte ptr [COLOR],al
;        CALL    INCHK                  ; Check for check
         CALL    INCHK
;        AND     al,al                  ; Is enemy in check ?
         AND     al,al
;        LD      al,ch                  ; Restore color
         MOV     al,ch
;        LD      (COLOR),al
         MOV     byte ptr [COLOR],al
;        JR      Z,CP24                 ; No - return
         JZ      CP24
;        CARRET                         ; New line
         CARRET
;        LD      al,(SCORE+1)           ; Check for player mated
         MOV     al,byte ptr [SCORE+1]
;        CMP     al,0FFH                ; Forced mate ?
         CMP     al,0FFH
;        CALL    NZ,TBCPMV              ; No - Tab to computer column
         JZ      skip31
         CALL    TBCPMV
skip31:
;        PRTBLK  CKMSG,5                ; Output "check"
         PRTBLK  CKMSG,5
;        LD      bx,LINECT              ; Address of screen line count
         MOV     ebx,offset LINECT
;        INC     (bx)                   ; Increment for message
         INC     byte ptr [ebx]
;CP24:   LD      al,(SCORE+1)           ; Check again for mates
CP24:    MOV     al,byte ptr [SCORE+1]
;        CMP     al,0FFH                ; Player mated ?
         CMP     al,0FFH
;        RET     NZ                     ; No - return
         JZ      skip32
         RET
skip32:
;        LD      cl,0                   ; Set player mate flag
         MOV     cl,0
;        CALL    FCDMAT                 ; Full checkmate ?
         CALL    FCDMAT
;        RET                            ; Return
         RET


;X p75
;**********************************************************
; FORCED MATE HANDLING
;**********************************************************
; FUNCTION: --  To examine situations where there exits
;               a forced mate and determine whether or
;               not the current move is checkmate. If it is,
;               a losing player is offered another game,
;               while a loss for the computer signals the
;               King to tip over in resignation.
;
; CALLED BY: -- CPTRMV
;
; CALLS: --     MATED
;               CHARTR
;               TBPLMV
;
; ARGUMENTS: -- The only value passed in a register is the
;               flag which tells FCDMAT whether the computer
;               or the player is mated.
;**********************************************************

;X p76
;*****************************************************************
; TAB TO PLAYERS COLUMN
;*****************************************************************
; FUNCTION: --  To space over in the move listing to the
;               column in which the players moves are being
;               recorded. This routine also reprints the
;               move number.
;
; CALLED BY: -- PLYRMV
;
; CALLS: --     None
;
; MACRO CALLS:  PRTBLK
;
; ARGUMENTS: -- None
;*****************************************************************

;*****************************************************************
; TAB TO COMPUTERS COLUMN
;*****************************************************************
; FUNCTION: --  To space over in the move listing to the
;               column in which the computers moves are
;               being recorded. This routine also reprints
;               the move number.
;
; CALLED BY: -- DRIVER
;               CPTRMV
;
; CALLS: --     None
;
; MACRO CALLS:  PRTBLK
;
; ARGUMENTS: -- None
;*****************************************************************

;X p77
;*****************************************************************
; TAB TO PLAYERS COLUMN W/0 MOVE NO.
;*****************************************************************
; FUNCTION: --  Like TBPLCL, except that the move number
;               is not reprinted.
;
; CALLED BY: -- FCDMAT
;***************************************************************

;*****************************************************************
; TAB TO COMPUTERS COLUMN W/O MOVE NO.
;*****************************************************************
; FUNCTION: --  Like TBCPCL, except that the move number
;               is not reprinted.
;
; CALLED BY: -- CPTRMV
;*****************************************************************


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
;BITASN: SUB     al,al                  ; Get ready for division
BITASN:  SUB     al,al
;        LD      dl,10
         MOV     dl,10
;        CALL    DIVIDE                 ; Divide
         CALL    DIVIDE
;        DEC     dh                     ; Get rank on 1-8 basis
         DEC     dh
;        ADD     al,60H                 ; Convert file to Ascii (a-h)
         ADD     al,60H
;        LD      bl,al                  ; Save
         MOV     bl,al
;        LD      al,dh                  ; Rank
         MOV     al,dh
;        ADD     al,30H                 ; Convert rank to Ascii (1-8)
         ADD     al,30H
;        LD      bh,al                  ; Save
         MOV     bh,al
;        RET                            ; Return
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
;ASNTBI: LD      al,bl                  ; Ascii rank (1 - 8)
ASNTBI:  MOV     al,bl
;        SUB     al,30H                 ; Rank 1 - 8
         SUB     al,30H
;        CMP     al,1                   ; Check lower bound
         CMP     al,1
;        JMP     M,AT04                 ; Jump if invalid
         JS      AT04
;        CMP     al,9                   ; Check upper bound
         CMP     al,9
;        JR      NC,AT04                ; Jump if invalid
         JNC     AT04
;        INC     al                     ; Rank 2 - 9
         INC     al
;        LD      dh,al                  ; Ready for multiplication
         MOV     dh,al
;        LD      dl,10
         MOV     dl,10
;        CALL    MLTPLY                 ; Multiply
         CALL    MLTPLY
;        LD      al,bh                  ; Ascii file letter (a - h)
         MOV     al,bh
;        SUB     al,40H                 ; File 1 - 8
         SUB     al,40H
;        CMP     al,1                   ; Check lower bound
         CMP     al,1
;        JMP     M,AT04                 ; Jump if invalid
         JS      AT04
;        CMP     al,9                   ; Check upper bound
         CMP     al,9
;        JR      NC,AT04                ; Jump if invalid
         JNC     AT04
;        ADD     al,dh                  ; File+Rank(20-90)=Board index
         ADD     al,dh
;        LD      ch,0                   ; Ok flag
         MOV     ch,0
;        RET                            ; Return
         RET
;AT04:   LD      ch,al                  ; Invalid flag
AT04:    MOV     ch,al
;        RET                            ; Return
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
;VALMOV: LD      bx,(MLPTRJ)            ; Save last move pointer
VALMOV:  MOV     ebx,[MLPTRJ]
;        PUSH    bx                     ; Save register
         PUSH ebx
;        LD      al,(KOLOR)             ; Computers color
         MOV     al,byte ptr [KOLOR]
;        XOR     al,80H                 ; Toggle color
         XOR     al,80H
;        LD      (COLOR),al             ; Store
         MOV     byte ptr [COLOR],al
;        LD      bx,PLYIX-2             ; Load move list index
         MOV     ebx,offset PLYIX-2
;        LD      (MLPTRI),bx
         MOV     [MLPTRI],ebx
;        LD      bx,MLIST+1024          ; Next available list pointer
         MOV     ebx,offset MLIST+1024
;        LD      (MLNXT),bx
         MOV     [MLNXT],ebx
;        CALL    GENMOV                 ; Generate opponents moves
         CALL    GENMOV
;        LD      si,MLIST+1024          ; Index to start of moves
         MOV     esi,offset MLIST+1024
;VA5:    LD      al,(MVEMSG)            ; "From" position
VA5:     MOV     al,byte ptr [MVEMSG]
;        CMP     al,(si+MLFRP)          ; Is it in list ?
         CMP     al,byte ptr [esi+MLFRP]
;        JR      NZ,VA6                 ; No - jump
         JNZ     VA6
;        LD      al,(MVEMSG+1)          ; "To" position
         MOV     al,byte ptr [MVEMSG+1]
;        CMP     al,(si+MLTOP)          ; Is it in list ?
         CMP     al,byte ptr [esi+MLTOP]
;        JR      Z,VA7                  ; Yes - jump
         JZ      VA7
;VA6:    LD      dl,(si+MLPTR)          ; Pointer to next list move
VA6:     MOV     dl,byte ptr [esi+MLPTR]
;        LD      dh,(si+MLPTR+1)
         MOV     dh,byte ptr [esi+MLPTR+1]
;        XOR     al,al                  ; At end of list ?
         XOR     al,al
;        CMP     al,dh
         CMP     al,dh
;        JR      Z,VA10                 ; Yes - jump
         JZ      VA10
;        PUSH    dx                     ; Move to X register
         PUSH edx
;        POP     si
         POP esi
;        JR      VA5                    ; Jump
         JMP     VA5
;VA7:    LD      (MLPTRJ),si            ; Save opponents move pointer
VA7:     MOV     [MLPTRJ],esi
;        CALL    MOVE                   ; Make move on board array
         CALL    MOVE
;        CALL    INCHK                  ; Was it a legal move ?
         CALL    INCHK
;        AND     al,al
         AND     al,al
;        JR      NZ,VA9                 ; No - jump
         JNZ     VA9
;VA8:    POP     bx                     ; Restore saved register
VA8:     POP ebx
;        RET                            ; Return
         RET
;VA9:    CALL    UNMOVE                 ; Un-do move on board array
VA9:     CALL    UNMOVE
;VA10:   LD      al,1                   ; Set flag for invalid move
VA10:    MOV     al,1
;        POP     bx                     ; Restore saved register
         POP ebx
;        LD      (MLPTRJ),bx            ; Save move pointer
         MOV     [MLPTRJ],ebx
;        RET                            ; Return
         RET

;X p82
;*************************************************************
; ACCEPT INPUT CHARACTER
;*************************************************************
; FUNCTION: --  Accepts a single character input from the
;               console keyboard and places it in the A
;               register. The character is also echoed on
;               the video screen, unless it is a carriage
;               return, line feed, or backspace. Lower case
;               alphabetic characters are folded to upper case.
;
; CALLED BY: -- DRIVER
;               INTERR
;               PLYRMV
;               ANALYS
;
; CALLS: --     None
;
; ARGUMENTS: -- Character input is output in register A.
;
; NOTES: --     This routine contains a reference to a
;               monitor function of the Jove monitor, there-
;               for the first few lines of this routine are
;               system dependent.
;*************************************************************

;X p83
;**********************************************************
; NEW PAGE IF NEEDED
;**********************************************************
; FUNCTION: --  To clear move list output when the column
;               has been filled.
;
; CALLED BY: -- DRIVER
;               PLYRMV
;               CPTRMV
;
; CALLS: --     DSPBRD
;
; ARGUMENTS: -- Returns a 1 in the A register if a new
;               page was turned.
;**********************************************************

;X p84
;*************************************************************
; DISPLAY MATED KING
;*************************************************************
; FUNCTION: --  To tip over the computers King when
;               mated.
;
; CALLED BY: -- FCDMAT
;
; CALLS: --     CONVRT
;               BLNKER
;               INSPCE (Abnormal Call to IP04)
;
; ARGUMENTS: -- None
;*************************************************************

;X p85
;**********************************************************
; SET UP POSITION FOR ANALYSIS
;**********************************************************
; FUNCTION: --  To enable user to set up any position
;               for analysis, or to continue to play
;               the game. The routine blinks the board
;               squares in turn and the user has the option
;               of leaving the contents unchanged by a
;               carriage return, emptying the square by a 0,
;               or inputting a piece of his chosing. To
;               enter a piece, type in piece-code,color-code,
;               moved-code.
;
;               Piece-code is a letter indicating the
;               desired piece:
;                       K -     King
;                       Q -     Queen
;                       R -     Rook
;                       B -     Bishop
;                       N -     Knight
;                       P -     Pawn
;
;               Color code is a letter, W for white, or B for
;               black.
;
;               Moved-code is a number. 0 indicates the piece has never
;               moved. 1 indicates the piece has moved.
;
;               A backspace will back up in the sequence of blinked
;               squares. An Escape will terminate the blink cycle and
;               verify that the position is correct, then procede
;               with game initialization.
;
; CALLED BY: -- DRIVER
;
; CALLS: --     CHARTR
;               DPSBRD
;               BLNKER
;               ROYALT
;               PLYRMV
;               CPTRMV
;
; MACRO CALLS:  PRTLIN
;               EXIT
;               CLRSCR
;               PRTBLK
;               CARRET
;
; ARGUMENTS: -- None
;**********************************************************
;X p86
;X p87

        
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
;ROYALT: LD      bx,POSK                ; Start of Royalty array
ROYALT:  MOV     ebx,offset POSK
;        LD      ch,4                   ; Clear all four positions
         MOV     ch,4
;back06: LD      (bx),0
back06:  MOV     byte ptr [ebx],0
;        INC     bx
         LEA     ebx,[ebx+1]
;        DJNZ    back06
         LAHF
         DEC ch
         JNZ     back06
         SAHF
;        LD      al,21                  ; First board position
         MOV     al,21
;RY04:   LD      (M1),al                ; Set up board index
RY04:    MOV     byte ptr [M1],al
;        LD      bx,POSK                ; Address of King position
         MOV     ebx,offset POSK
;        LD      si,(M1)
         MOV     esi,[M1]
;        LD      al,(si+BOARD)          ; Fetch board contents
         MOV     al,byte ptr [esi+BOARD]
;        BIT     7,al                   ; Test color bit
         TEST    al,80h
;        JR      Z,rel023               ; Jump if white
         JZ      rel023
;        INC     bx                     ; Offset for black
         LEA     ebx,[ebx+1]
;rel023: AND     al,7                   ; Delete flags, leave piece
rel023:  AND     al,7
;        CMP     al,KING                ; King ?
         CMP     al,KING
;        JR      Z,RY08                 ; Yes - jump
         JZ      RY08
;        CMP     al,QUEEN               ; Queen ?
         CMP     al,QUEEN
;        JR      NZ,RY0C                ; No - jump
         JNZ     RY0C
;        INC     bx                     ; Queen position
         LEA     ebx,[ebx+1]
;        INC     bx                     ; Plus offset
         LEA     ebx,[ebx+1]
;RY08:   LD      al,(M1)                ; Index
RY08:    MOV     al,byte ptr [M1]
;        LD      (bx),al                ; Save
         MOV     byte ptr [ebx],al
;RY0C:   LD      al,(M1)                ; Current position
RY0C:    MOV     al,byte ptr [M1]
;        INC     al                     ; Next position
         INC     al
;        CMP     al,99                  ; Done.?
         CMP     al,99
;        JR      NZ,RY04                ; No - jump
         JNZ     RY04
;        RET                            ; Return
         RET

;X p89
;*************************************************************
; SET UP EMPTY BOARD
;*************************************************************
; FUNCTION: --  Diplay graphics board and pieces.
;
; CALLED BY: -- DRIVER
;               ANALYS
;               PGIFND
;
; CALLS: --     CONVRT
;               INSPCE
;
; ARGUMENTS: -- None
;
; NOTES: --     This routine makes use of several fixed
;               addresses in the video storage area of
;               the Jupiter III computer, and is therefor
;               system dependent. Each such reference will
;               be marked.
;*************************************************************
                                ; address
;X p90

;X p91
;**********************************************************
; INSERT PIECE SUBROUTINE
;**********************************************************
; FUNCTION: --  This subroutine places a piece onto a
;               given square on the video board. The piece
;               inserted is that stored in the board array
;               for that square.
;
; CALLED BY: -- DPSPRD
;               MATED
;
; CALLS: --     MLTPLY
;
; ARGUMENTS: -- Norm address for the square in register
;               pair HL.
;**********************************************************
;X p92


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
;CONVRT: PUSH    cx                     ; Save registers
CONVRT:  PUSH ecx
;        PUSH    dx
         PUSH edx
;        PUSH    af
         lahf
         PUSH eax
;        LD      al,(BRDPOS)            ; Get board index
         MOV     al,byte ptr [BRDPOS]
;        LD      dh,al                  ; Set up dividend
         MOV     dh,al
;        SUB     al,al
         SUB     al,al
;        LD      dl,10                  ; Divisor
         MOV     dl,10
;        CALL    DIVIDE                 ; Index into rank and file
         CALL    DIVIDE
                        ; file (1-8) & rank (2-9)
;        DEC     dh                     ; For rank (1-8)
         DEC     dh
;        DEC     al                     ; For file (0-7)
         DEC     al
;        LD      cl,dh                  ; Save
         MOV     cl,dh
;        LD      dh,6                   ; Multiplier
         MOV     dh,6
;        LD      dl,al                  ; File number is multiplicand
         MOV     dl,al
;        CALL    MLTPLY                 ; Giving file displacement
         CALL    MLTPLY
;        LD      al,dh                  ; Save
         MOV     al,dh
;        ADD     al,10H                 ; File norm address
         ADD     al,10H
;        LD      bl,al                  ; Low order address byte
         MOV     bl,al
;        LD      al,8                   ; Rank adjust
         MOV     al,8
;        SUB     al,cl                  ; Rank displacement
         SUB     al,cl
;        ADD     al,0C0H                ; Rank Norm address
         ADD     al,0C0H
;        LD      bh,al                  ; High order addres byte
         MOV     bh,al
;        POP     af                     ; Restore registers
         POP eax
         sahf
;        POP     dx
         POP edx
;        POP     cx
         POP ecx
;        RET                            ; Return
         RET

;X p94
;**********************************************************
; POSITIVE INTEGER DIVISION
;**********************************************************
;DIVIDE: PUSH    cx
DIVIDE:  PUSH ecx
;        LD      ch,8
         MOV     ch,8
;DD04:   SLA     dh
DD04:    SHL     dh,1
;        RLA
         RCL     al,1
;        SUB     al,dl
         SUB     al,dl
;        JMP     M,rel024
         JS      rel024
;        INC     dh
         INC     dh
;        JR      rel024
         JMP     rel024
;        ADD     al,dl
         ADD     al,dl
;rel024: DJNZ    DD04
rel024:  LAHF
         DEC ch
         JNZ     DD04
         SAHF
;        POP     cx
         POP ecx
;        RET
         RET

;**********************************************************
; POSITIVE INTEGER MULTIPLICATION
;**********************************************************
;MLTPLY: PUSH    cx
MLTPLY:  PUSH ecx
;        SUB     al,al
         SUB     al,al
;        LD      ch,8
         MOV     ch,8
;ML04:   BIT     0,dh
ML04:    TEST    dh,1
;        JR      Z,rel025
         JZ      rel025
;        ADD     al,dl
         ADD     al,dl
;rel025: SRA     al
rel025:  SAR     al,1
;        RR      dh
         RCR     dh,1
;        DJNZ    ML04
         LAHF
         DEC ch
         JNZ     ML04
         SAHF
;        POP     cx
         POP ecx
;        RET
         RET

;X p95
;**********************************************************
; SQUARE BLINKER
;**********************************************************
;
; FUNCTION: --  To blink the graphics board square to signal
;               a piece's intention to move, or to high-
;               light the square as being alterable
;               in ANALYS.
;
; CALLED BY: -- MAKEMV
;               ANALYS
;               MATED
;
; CALLS: --     None
;
; ARGUMENTS: -- Norm address of desired square passed in register
;               pair HL. Number of times to blink passed in
;               register B.
;**********************************************************
;X p96


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
;EXECMV: PUSH    si                     ; Save registers
EXECMV:  PUSH esi
;        PUSH    af
         lahf
         PUSH eax
;        LD      si,(MLPTRJ)            ; Index into move list
         MOV     esi,[MLPTRJ]
;        LD      cl,(si+MLFRP)          ; Move list "from" position
         MOV     cl,byte ptr [esi+MLFRP]
;        LD      dl,(si+MLTOP)          ; Move list "to" position
         MOV     dl,byte ptr [esi+MLTOP]
;        CALL    MAKEMV                 ; Produce move
         CALL    MAKEMV
;        LD      dh,(si+MLFLG)          ; Move list flags
         MOV     dh,byte ptr [esi+MLFLG]
;        LD      ch,0
         MOV     ch,0
;        BIT     6,dh                   ; Double move ?
         TEST    dh,40h
;        JR      Z,EX14                 ; No - jump
         JZ      EX14
;        LD      dx,8                   ; Move list entry width
         MOV     edx,8
;        ADD     si,dx                  ; Increment MLPTRJ
         LEA     esi,[esi+edx]
;        LD      cl,(si+MLFRP)          ; Second "from" position
         MOV     cl,byte ptr [esi+MLFRP]
;        LD      dl,(si+MLTOP)          ; Second "to" position
         MOV     dl,byte ptr [esi+MLTOP]
;        LD      al,dl                  ; Get "to" position
         MOV     al,dl
;        CMP     al,cl                  ; Same as "from" position ?
         CMP     al,cl
;        JR      NZ,EX04                ; No - jump
         JNZ     EX04
;        INC     ch                     ; Set en passant flag
         INC     ch
;        JR      EX10                   ; Jump
         JMP     EX10
;EX04:   CMP     al,1AH                 ; White O-O ?
EX04:    CMP     al,1AH
;        JR      NZ,EX08                ; No - jump
         JNZ     EX08
;        SET     1,ch                   ; Set O-O flag
         LAHF
         OR      ch,2
         SAHF
;        JR      EX10                   ; Jump
         JMP     EX10
;EX08:   CMP     al,60H                 ; Black 0-0 ?
EX08:    CMP     al,60H
;        JR      NZ,EX0C                ; No - jump
         JNZ     EX0C
;        SET     1,ch                   ; Set 0-0 flag
         LAHF
         OR      ch,2
         SAHF
;        JR      EX10                   ; Jump
         JMP     EX10
;EX0C:   SET     2,ch                   ; Set 0-0-0 flag
EX0C:    LAHF
         OR      ch,4
         SAHF
;EX10:   CALL    MAKEMV                 ; Make 2nd move on board
EX10:    CALL    MAKEMV
;EX14:   POP     af                     ; Restore registers
EX14:    POP eax
         sahf
;        POP     si
         POP esi
;        RET                            ; Return
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
;MAKEMV: PUSH    af                     ; Save register
MAKEMV:  lahf
         PUSH eax
;        PUSH    cx
         PUSH ecx
;        PUSH    dx
         PUSH edx
;        PUSH    bx
         PUSH ebx
;        LD      al,cl                  ; "From" position
         MOV     al,cl
;        LD      (BRDPOS),al            ; Set up parameter
         MOV     byte ptr [BRDPOS],al
;        CALL    CONVRT                 ; Getting Norm address in HL
         CALL    CONVRT
;        LD      ch,10                  ; Blink parameter
         MOV     ch,10
;        CALL    BLNKER                 ; Blink "from" square
         CALL    BLNKER
;        LD      al,(bx)                ; Bring in Norm 1plock
         MOV     al,byte ptr [ebx]
;        INC     bl                     ; First change block
         INC     bl
;        LD      dh,0                   ; Bar counter
         MOV     dh,0
;MM04:   LD      ch,4                   ; Block counter
MM04:    MOV     ch,4
;MM08:   LD      (bx),al                ; Insert blank block
MM08:    MOV     byte ptr [ebx],al
;        INC     bl                     ; Next change block
         INC     bl
;        DJNZ    MM08                   ; Done ? No - jump
         LAHF
         DEC ch
         JNZ     MM08
         SAHF
;        LD      cl,al                  ; Saving norm block
         MOV     cl,al
;        LD      al,bl                  ; Bar increment
         MOV     al,bl
;        ADD     al,3CH
         ADD     al,3CH
;        LD      bl,al
         MOV     bl,al
;        LD      al,cl                  ; Restore Norm block
         MOV     al,cl
;        INC     dh
         INC     dh
;        BIT     2,dh                   ; Done ?
         TEST    dh,4
;        JR      Z,MM04                 ; No - jump
         JZ      MM04
;        LD      al,dl                  ; Get "to" position
         MOV     al,dl
;        LD      (BRDPOS),al            ; Set up parameter
         MOV     byte ptr [BRDPOS],al
;        CALL    CONVRT                 ; Getting Norm address in HL
         CALL    CONVRT
;        LD      ch,10                  ; Blink parameter
         MOV     ch,10
;        CALL    INSPCE                 ; Inserts the piece
         CALL    INSPCE
;        CALL    BLNKER                 ; Blinks "to" square
         CALL    BLNKER
;        POP     bx                     ; Restore registers
         POP ebx
;        POP     dx
         POP edx
;        POP     cx
         POP ecx
;        POP     af
         POP eax
         sahf
;        RET                            ; Return
         RET
DISPATCH ENDP

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

;   SUB     A               ; Code of White is zero
    sub al,al
;   STA     COLOR           ; White always moves first
    mov byte ptr [COLOR],al
;   STA     KOLOR           ; Bring in computer's color
    mov byte ptr [KOLOR],al
;   CALL    INTERR          ; Players color/search depth
;   call    INTERR
    mov byte ptr [PLYMAX],1
    mov al,0
;   CALL    INITBD          ; Initialize board array
    call    DISPATCH
;   MVI     A,1             ; Move number is 1 at at start
    mov al,1
;   STA     MOVENO          ; Save
    mov byte ptr [MOVENO],al
;   CALL    CPTRMV          ; Make and write computers move
    mov al,1
    call    DISPATCH
    pop     edi
    pop     esi
    pop     ebp
	ret
_shim_function ENDP

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
