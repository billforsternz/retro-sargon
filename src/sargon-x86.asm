;***********************************************************
;
;               SARGON
;
;       Sargon is a computer chess playing program designed
; and coded by Dan and Kathe Spracklen.  Copyright 1978. All
; rights reserved.  No part of this publication may be
; reproduced without the prior written permission.
;**********************************************************

        .686P
        .XMM
        .model  flat

;***********************************************************
; EQUATES
;***********************************************************
;
PAWN    EQU     1
KNIGHT  EQU     2
BISHOP  EQU     3
ROOK    EQU     4
QUEEN   EQU     5
KING    EQU     6
WHITE   EQU     0
BLACK   EQU     80H
BPAWN   EQU     BLACK+PAWN

;***********************************************************
; TABLES SECTION
;***********************************************************
_DATA   SEGMENT
PUBLIC  _sargon_base_address
_sargon_base_address:
;       ORG     100h
        DB      256     DUP (?)                 ;Padding bytes to ORG location
TBASE   EQU     0100h
;TBASE must be page aligned, but not page 0, because an
;extensively used trick is to test whether the hi byte of
;a pointer == 0 and to consider this as a equivalent to
;testing whether the whole pointer == 0 (works as long as
;pointers never point to page 0). Also there is an apparent
;bug in Sargon, such that MLPTRJ is left at 0 for the root
;node and the MLVAL for that root node is therefore written
;to memory at offset 5 from 0 (so in page 0). It's a bit
;wasteful to waste a whole 256 byte page for this, but it
;is compatible with the goal of making as few changes as
;possible to the inner heart of Sargon.
;**********************************************************
; DIRECT  --  Direction Table.  Used to determine the dir-
;             ection of movement of each piece.
;***********************************************************
DIRECT  EQU     0100h-TBASE
        DB      +09,+11,-11,-09
        DB      +10,-10,+01,-01
        DB      -21,-12,+08,+19
        DB      +21,+12,-08,-19
        DB      +10,+10,+11,+09
        DB      -10,-10,-11,-09
;***********************************************************
; DPOINT  --  Direction Table Pointer. Used to determine
;             where to begin in the direction table for any
;             given piece.
;***********************************************************
DPOINT  EQU     0118h-TBASE
        DB      20,16,8,0,4,0,0

;***********************************************************
; DCOUNT  --  Direction Table Counter. Used to determine
;             the number of directions of movement for any
;             given piece.
;***********************************************************
DCOUNT  EQU     011fh-TBASE
        DB      4,4,8,4,4,8,8

;***********************************************************
; PVALUE  --  Point Value. Gives the point value of each
;             piece, or the worth of each piece.
;***********************************************************
PVALUE  EQU     0126h-TBASE-1
        DB      1,3,3,5,9,10

;***********************************************************
; PIECES  --  The initial arrangement of the first rank of
;             pieces on the board. Use to set up the board
;             for the start of the game.
;***********************************************************
PIECES  EQU     012ch-TBASE
        DB      4,2,3,5,6,3,2,4

;***********************************************************
; BOARD   --  Board Array.  Used to hold the current position
;             of the board during play. The board itself
;             looks like:
;             FFFFFFFFFFFFFFFFFFFF
;             FFFFFFFFFFFFFFFFFFFF
;             FF0402030506030204FF
;             FF0101010101010101FF
;             FF0000000000000000FF
;             FF0000000000000000FF
;             FF0000000000000060FF
;             FF0000000000000000FF
;             FF8181818181818181FF
;             FF8482838586838284FF
;             FFFFFFFFFFFFFFFFFFFF
;             FFFFFFFFFFFFFFFFFFFF
;             The values of FF form the border of the
;             board, and are used to indicate when a piece
;             moves off the board. The individual bits of
;             the other bytes in the board array are as
;             follows:
;             Bit 7 -- Color of the piece
;                     1 -- Black
;                     0 -- White
;             Bit 6 -- Not used
;             Bit 5 -- Not used
;             Bit 4 --Castle flag for Kings only
;             Bit 3 -- Piece has moved flag
;             Bits 2-0 Piece type
;                     1 -- Pawn
;                     2 -- Knight
;                     3 -- Bishop
;                     4 -- Rook
;                     5 -- Queen
;                     6 -- King
;                     7 -- Not used
;                     0 -- Empty Square
;***********************************************************
BOARD   EQU     0134h-TBASE
BOARDA  EQU     0134h
        DB      120     DUP (?)

;***********************************************************
; ATKLIST -- Attack List. A two part array, the first
;            half for white and the second half for black.
;            It is used to hold the attackers of any given
;            square in the order of their value.
;
; WACT   --  White Attack Count. This is the first
;            byte of the array and tells how many pieces are
;            in the white portion of the attack list.
;
; BACT   --  Black Attack Count. This is the eighth byte of
;            the array and does the same for black.
;***********************************************************
WACT    EQU     ATKLST
BACT    EQU     ATKLST+7
ATKLST  EQU     01ach
        DW      0,0,0,0,0,0,0

;***********************************************************
; PLIST   --  Pinned Piece Array. This is a two part array.
;             PLISTA contains the pinned piece position.
;             PLISTD contains the direction from the pinned
;             piece to the attacker.
;***********************************************************
PLIST   EQU     01bah-TBASE-1
PLISTD  EQU     PLIST+10
PLISTA  EQU     01bah
        DW      0,0,0,0,0,0,0,0,0,0

;***********************************************************
; POSK    --  Position of Kings. A two byte area, the first
;             byte of which hold the position of the white
;             king and the second holding the position of
;             the black king.
;
; POSQ    --  Position of Queens. Like POSK,but for queens.
;***********************************************************
POSK    EQU     01ceh
        DB      24,95
POSQ    EQU     01d0h
        DB      14,94
        DB      -1

;***********************************************************
; SCORE   --  Score Array. Used during Alpha-Beta pruning to
;             hold the scores at each ply. It includes two
;             "dummy" entries for ply -1 and ply 0.
;**********************************************************
;       ORG     200h
        DB      45      DUP (?)                 ;Padding bytes to ORG location
SCORE   EQU     0200h                           ;extended up to 10 ply
        DW      0,0,0,0,0,0,0,0,0,0,0

;***********************************************************
; PLYIX   --  Ply Table. Contains pairs of pointers, a pair
;             for each ply. The first pointer points to the
;             top of the list of possible moves at that ply.
;             The second pointer points to which move in the
;             list is the one currently being considered.
;***********************************************************
PLYIX   EQU     0216h
        DW      0,0,0,0,0,0,0,0,0,0
        DW      0,0,0,0,0,0,0,0,0,0

;***********************************************************
; STACK   --  Contains the stack for the program.
;***********************************************************
;For this C Callable Sargon, we just use the standard C Stack
;Significantly, Sargon doesn't do any stack based trickery
;just calls, returns, pushes and pops - so it shouldn't be a
;problem that we are doing these 32 bits at a time instead of 16

;***********************************************************
; TABLE INDICES SECTION
;
; M1-M4   --  Working indices used to index into
;             the board array.
;
; T1-T3   --  Working indices used to index into Direction
;             Count, Direction Value, and Piece Value tables.
;
; INDX1   --  General working indices. Used for various
; INDX2       purposes.
;
; NPINS   --  Number of Pins. Count and pointer into the
;             pinned piece list.
;
; MLPTRI  --  Pointer into the ply table which tells
;             which pair of pointers are in current use.
;
; MLPTRJ  --  Pointer into the move list to the move that is
;             currently being processed.
;
; SCRIX   --  Score Index. Pointer to the score table for
;             the ply being examined.
;
; BESTM   --  Pointer into the move list for the move that
;             is currently considered the best by the
;             Alpha-Beta pruning process.
;
; MLLST   --  Pointer to the previous move placed in the move
;             list. Used during generation of the move list.
;
; MLNXT   --  Pointer to the next available space in the move
;             list.
;
;***********************************************************
;       ORG     300h
        DB      194     DUP (?)                 ;Padding bytes to ORG location
M1      EQU     0300h
        DW      TBASE
M2      EQU     0302h
        DW      TBASE
M3      EQU     0304h
        DW      TBASE
M4      EQU     0306h
        DW      TBASE
T1      EQU     0308h
        DW      TBASE
T2      EQU     030ah
        DW      TBASE
T3      EQU     030ch
        DW      TBASE
INDX1   EQU     030eh
        DW      TBASE
INDX2   EQU     0310h
        DW      TBASE
NPINS   EQU     0312h
        DW      TBASE
MLPTRI  EQU     0314h
        DW      PLYIX
MLPTRJ  EQU     0316h
        DW      0
SCRIX   EQU     0318h
        DW      0
BESTM   EQU     031ah
        DW      0
MLLST   EQU     031ch
        DW      0
MLNXT   EQU     031eh
        DW      MLIST

;***********************************************************
; VARIABLES SECTION
;
; KOLOR   --  Indicates computer's color. White is 0, and
;             Black is 80H.
;
; COLOR   --  Indicates color of the side with the move.
;
; P1-P3   --  Working area to hold the contents of the board
;             array for a given square.
;
; PMATE   --  The move number at which a checkmate is
;             discovered during look ahead.
;
; MOVENO  --  Current move number.
;
; PLYMAX  --  Maximum depth of search using Alpha-Beta
;             pruning.
;
; NPLY    --  Current ply number during Alpha-Beta
;             pruning.
;
; CKFLG   --  A non-zero value indicates the king is in check.
;
; MATEF   --  A zero value indicates no legal moves.
;
; VALM    --  The score of the current move being examined.
;
; BRDC    --  A measure of mobility equal to the total number
;             of squares white can move to minus the number
;             black can move to.
;
; PTSL    --  The maximum number of points which could be lost
;             through an exchange by the player not on the
;             move.
;
; PTSW1   --  The maximum number of points which could be won
;             through an exchange by the player not on the
;             move.
;
; PTSW2   --  The second highest number of points which could
;             be won through a different exchange by the player
;             not on the move.
;
; MTRL    --  A measure of the difference in material
;             currently on the board. It is the total value of
;             the white pieces minus the total value of the
;             black pieces.
;
; BC0     --  The value of board control(BRDC) at ply 0.
;
; MV0     --  The value of material(MTRL) at ply 0.
;
; PTSCK   --  A non-zero value indicates that the piece has
;             just moved itself into a losing exchange of
;             material.
;
; BMOVES  --  Our very tiny book of openings. Determines
;             the first move for the computer.
;
;***********************************************************
KOLOR   EQU     0320h
        DB      0
COLOR   EQU     0321h
        DB      0
P1      EQU     0322h
        DB      0
P2      EQU     0323h
        DB      0
P3      EQU     0324h
        DB      0
PMATE   EQU     0325h
        DB      0
MOVENO  EQU     0326h
        DB      0
PLYMAX  EQU     0327h
        DB      2
NPLY    EQU     0328h
        DB      0
CKFLG   EQU     0329h
        DB      0
MATEF   EQU     032ah
        DB      0
VALM    EQU     032bh
        DB      0
BRDC    EQU     032ch
        DB      0
PTSL    EQU     032dh
        DB      0
PTSW1   EQU     032eh
        DB      0
PTSW2   EQU     032fh
        DB      0
MTRL    EQU     0330h
        DB      0
BC0     EQU     0331h
        DB      0
MV0     EQU     0332h
        DB      0
PTSCK   EQU     0333h
        DB      0
BMOVES  EQU     0334h
        DB      35,55,10H
        DB      34,54,10H
        DB      85,65,10H
        DB      84,64,10H
LINECT  EQU     0340h                           ;not needed in X86 port (but avoids assembler error)
        DB      0
MVEMSG  EQU     0341h                           ;not needed in X86 port (but avoids assembler error)
        DB      0,0,0,0,0
                                                ;(in Z80 Sargon user interface was algebraic move in ascii
                                                ;needs to be five bytes long)

;***********************************************************
; MOVE LIST SECTION
;
; MLIST   --  A 2048 byte storage area for generated moves.
;             This area must be large enough to hold all
;             the moves for a single leg of the move tree.
;
; MLEND   --  The address of the last available location
;             in the move list.
;
; MLPTR   --  The Move List is a linked list of individual
;             moves each of which is 6 bytes in length. The
;             move list pointer(MLPTR) is the link field
;             within a move.
;
; MLFRP   --  The field in the move entry which gives the
;             board position from which the piece is moving.
;
; MLTOP   --  The field in the move entry which gives the
;             board position to which the piece is moving.
;
; MLFLG   --  A field in the move entry which contains flag
;             information. The meaning of each bit is as
;             follows:
;             Bit 7  --  The color of any captured piece
;                        0 -- White
;                        1 -- Black
;             Bit 6  --  Double move flag (set for castling and
;                        en passant pawn captures)
;             Bit 5  --  Pawn Promotion flag; set when pawn
;                        promotes.
;             Bit 4  --  When set, this flag indicates that
;                        this is the first move for the
;                        piece on the move.
;             Bit 3  --  This flag is set is there is a piece
;                        captured, and that piece has moved at
;                        least once.
;             Bits 2-0   Describe the captured piece.  A
;                        zero value indicates no capture.
;
; MLVAL   --  The field in the move entry which contains the
;             score assigned to the move.
;
;***********************************************************


;       ORG     400h
        DB      186     DUP (?)                 ;Padding bytes to ORG location
MLIST   EQU     0400h
        DB      60000   DUP (?)
MLEND   EQU     0ee60h
        DW      0
MLPTR   EQU     0
MLFRP   EQU     2
MLTOP   EQU     3
MLFLG   EQU     4
MLVAL   EQU     5

;***********************************************************
shadow_ax  dw   0
shadow_bx  dw   0
shadow_cx  dw   0
shadow_dx  dw   0
_DATA    ENDS

;**********************************************************
; PROGRAM CODE SECTION
;**********************************************************
_TEXT   SEGMENT
EXTERN  _callback: PROC

;
; Miscellaneous stubs
;
FCDMAT:  RET
TBCPMV:  RET
MAKEMV:  RET
PRTBLK   MACRO   name,len
         ENDM
CARRET   MACRO
         ENDM

;
; Callback into C++ code (for debugging, report on progress etc.)
;
callback_enabled EQU 1
         IF callback_enabled
CALLBACK MACRO   txt
LOCAL    cb_end
         pushfd         ;save all registers, also can be inspected by callback()
         pushad
         call   _callback
         jmp    cb_end
         db     txt,0
cb_end:  popad
         popfd
         ENDM
         ELSE
CALLBACK MACRO   txt
         ENDM
         ENDIF

;
; Z80 Opcode emulation
;

Z80_EXAF MACRO
         lahf
         xchg    ax,shadow_ax
         sahf
         ENDM

Z80_EXX  MACRO
         xchg    bx,shadow_bx
         xchg    cx,shadow_cx
         xchg    dx,shadow_dx
         ENDM

Z80_RLD  MACRO                          ;a=kx (hl)=yz -> a=ky (hl)=zx
         mov     ah,byte ptr [ebp+ebx]  ;ax=yzkx
         ror     al,4                   ;ax=yzxk
         rol     ax,4                   ;ax=zxky
         mov     byte ptr [ebp+ebx],ah  ;al=ky [ebx]=zx
         or      al,al                  ;set z and s flags
         ENDM

Z80_RRD  MACRO                          ;a=kx (hl)=yz -> a=kz (hl)=xy
         mov     ah,byte ptr [ebp+ebx]  ;ax=yzkx
         ror     ax,4                   ;ax=xyzk
         ror     al,4                   ;ax=xykz
         mov     byte ptr [ebp+ebx],ah  ;al=kz [ebx]=xy
         or      al,al                  ;set z and s flags
         ENDM

Z80_LDAR MACRO                          ;to get random number
LOCAL    ldar_1
LOCAL    ldar_2
         pushf                          ;maybe there's entropy in stack junk
         push    ebx
         mov     ebx,esp
         mov     ax,0
ldar_1:  xor     al,byte ptr [ebx]
         dec     ebx
         jz      ldar_2
         dec     ah
         jnz     ldar_1
ldar_2:  pop     ebx
         popf
         ENDM

Z80_CPIR MACRO
;CPIR reference, from the Zilog Z80 Users Manual
;A - (HL), HL => HL+1, BC => BC - 1
;If decrementing causes BC to go to 0 or if A = (HL), the instruction is terminated.
;P/V is set if BC - 1 does not equal 0; otherwise, it is reset.
;
;So result of the subtraction discarded, but flags are set, (although CY unaffected).
;*BUT* P flag (P/V flag in Z80 parlance as it serves double duty as an overflow
;flag after some instructions in that CPU) reflects the result of decrementing BC
;rather than A - (HL)
;
;We support reflecting the result in the Z and P flags. The possibilities are
;Z=1 P=1 (Z and PE)  -> first match found, counter hasn't expired
;Z=1 P=0 (Z and PO)  -> match found in last position, counter has expired
;Z=0 P=0 (NZ and PO) -> no match found, counter has expired
;
;Notes on the parity bit
;
;Parity bit in flags is set if number of 1s in lsb is even
;Parity bit in flags is cleared if number of 1s in lsb is odd
;Mnemonics to jump if flag set (parity even);
;8080: JPE dest
;Z80:  JMP PE,dest
;X86:  JPE dest
;Mnemonics to jump if flag clear (parity odd);
;8080: JPO dest
;Z80:  JMP PO,dest
;X86:  JPO dest
;AH format after LAHF = SF:ZF:0:AF:0:PF:1:CF (so bit 6=ZF, bit 2=PF)

LOCAL    cpir_1
LOCAL    cpir_2
LOCAL    cpir_3
LOCAL    cpir_end
cpir_1:  dec     cx                 ;Counter decrements regardless
         inc     bx                 ;Address increments regardless
         cmp     al,byte ptr [ebp+ebx-1]  ;Compare
         jcxz    cpir_2             ;Handle CX eq 0 and ne 0 separately

         ;CX is not zero (common case)
         jnz    cpir_1              ;continue search (common case)
         xor    ah,ah               ;End with Z (found) and PE (counter hadn't expired)
         jmp    cpir_end

         ;CX is zero
cpir_2:  mov    ah,42h              ;If Z, end with Z (found) and PO (counter expired)
                                    ;01000010  Z is bit 6 set, PO is bit 2 clear
                                    ;PF bit clear means PO
                                    ;note respecting bit 1 always set after lahf
                                    ;(hard to organise combination of Z and PO except by
                                    ;using sahf because 0 has even parity)
         jz     cpir_3              ;
         mov    ah,02h              ;If NZ, end with NZ (not found) and PO (counter expired)
cpir_3:  sahf
cpir_end:
         ENDM

;Wrap all code in a PROC to get source debugging
PUBLIC   _sargon
_sargon  PROC
         push   eax
         push   ebx
         push   ecx
         push   edx
         push   esi
         push   edi
         push   ebp              ;sp -> ebp,edi,esi,edx,ecx,ebx,eax,ret_addr,parm1,parm2
                                 ;      +0, +4 ,+8, +12,+16,+20,+24,+28,    ,+32  ,+36
         mov    ebp,[esp+36]     ;parm2 = ptr to REGS
         ;We are going to use 32 bit registers as 16 bit ptrs - hi 16 bits should always be zero
         xor    eax,eax
         xor    ebx,ebx
         xor    ecx,ecx
         xor    edx,edx
         xor    esi,esi
         xor    edi,edi
         cmp    ebp,0
         jz     reg_1
         mov    ax, word ptr [ebp];
         mov    bx, word ptr [ebp+2];
         mov    cx, word ptr [ebp+4];
         mov    dx, word ptr [ebp+6];
         mov    si, word ptr [ebp+8];
         mov    di, word ptr [ebp+10];
reg_1:   lea    ebp,_sargon_base_address
         cmp    dword ptr [esp+32],1     ;parm1 = command code, 1=INITBD etc
         jz     api_1_INITBD
         cmp    dword ptr [esp+32],2
         jz     api_2_ROYALT
         cmp    dword ptr [esp+32],3
         jz     api_3_CPTRMV
         cmp    dword ptr [esp+32],4
         jz     api_4_VALMOV
         cmp    dword ptr [esp+32],5
         jz     api_5_ASNTBI
         cmp    dword ptr [esp+32],6
         jz     api_6_EXECMV
         jmp    api_end

api_1_INITBD:
         sahf
         call   INITBD
         jmp    api_end
api_2_ROYALT:
         sahf
         call   ROYALT
         jmp    api_end
api_3_CPTRMV:
         sahf
         call   CPTRMV
         jmp    api_end
api_4_VALMOV:
         sahf
         call   VALMOV
         jmp    api_end
api_5_ASNTBI:
         sahf
         call   ASNTBI
         jmp    api_end
api_6_EXECMV:
         sahf
         call   EXECMV
         jmp    api_end

api_end: mov    ebp,[esp+36]     ;parm2 = ptr to REGS
         cmp    ebp,0
         jz     reg_2
         lahf
         mov    word ptr [ebp], ax
         mov    word ptr [ebp+2], bx
         mov    word ptr [ebp+4], cx
         mov    word ptr [ebp+6], dx
         mov    word ptr [ebp+8], si
         mov    word ptr [ebp+10], di
reg_2:   pop    ebp
         pop    edi
         pop    esi
         pop    edx
         pop    ecx
         pop    ebx
         pop    eax
         ret

;**********************************************************
; BOARD SETUP ROUTINE
;***********************************************************
; FUNCTION:   To initialize the board array, setting the
;             pieces in their initial positions for the
;             start of the game.
;
; CALLED BY:  DRIVER
;
; CALLS:      None
;
; ARGUMENTS:  None
;***********************************************************
INITBD: MOV     ch,120                          ; Pre-fill board with -1's
        MOV     bx,BOARDA
back01: MOV     byte ptr [ebp+ebx],-1
        LAHF
        INC     bx
        SAHF
        LAHF
        DEC     ch
        JNZ     back01
        SAHF
        MOV     ch,8
        MOV     si,BOARDA
IB2:    MOV     al,byte ptr [ebp+esi-8]         ; Fill non-border squares
        MOV     byte ptr [ebp+esi+21],al        ; White pieces
        LAHF                                    ; Change to black
        OR      al,80h
        SAHF
        MOV     byte ptr [ebp+esi+91],al        ; Black pieces
        MOV     byte ptr [ebp+esi+31],PAWN      ; White Pawns
        MOV     byte ptr [ebp+esi+81],BPAWN     ; Black Pawns
        MOV     byte ptr [ebp+esi+41],0         ; Empty squares
        MOV     byte ptr [ebp+esi+51],0
        MOV     byte ptr [ebp+esi+61],0
        MOV     byte ptr [ebp+esi+71],0
        LAHF
        INC     si
        SAHF
        LAHF
        DEC     ch
        JNZ     IB2
        SAHF
        MOV     si,POSK                         ; Init King/Queen position list
        MOV     byte ptr [ebp+esi+0],25
        MOV     byte ptr [ebp+esi+1],95
        MOV     byte ptr [ebp+esi+2],24
        MOV     byte ptr [ebp+esi+3],94
        RET

;***********************************************************
; PATH ROUTINE
;***********************************************************
; FUNCTION:   To generate a single possible move for a given
;             piece along its current path of motion including:

;                Fetching the contents of the board at the new
;                position, and setting a flag describing the
;                contents:
;                          0  --  New position is empty
;                          1  --  Encountered a piece of the
;                                 opposite color
;                          2  --  Encountered a piece of the
;                                 same color
;                          3  --  New position is off the
;                                 board
;
; CALLED BY:  MPIECE
;             ATTACK
;             PINFND
;
; CALLS:      None
;
; ARGUMENTS:  Direction from the direction array giving the
;             constant to be added for the new position.
;***********************************************************
PATH:   MOV     bx,M2                           ; Get previous position
        MOV     al,byte ptr [ebp+ebx]
        ADD     al,cl                           ; Add direction constant
        MOV     byte ptr [ebp+ebx],al           ; Save new position
        MOV     si,word ptr [ebp+M2]            ; Load board index
        MOV     al,byte ptr [ebp+esi+BOARD]     ; Get contents of board
        CMP     al,-1                           ; In border area ?
        JZ      PA2                             ; Yes - jump
        MOV     byte ptr [ebp+P2],al            ; Save piece
        AND     al,7                            ; Clear flags
        MOV     byte ptr [ebp+T2],al            ; Save piece type
        JNZ     skip1                           ; Return if empty
        RET
skip1:
        MOV     al,byte ptr [ebp+P2]            ; Get piece encountered
        MOV     bx,P1                           ; Get moving piece address
        XOR     al,byte ptr [ebp+ebx]           ; Compare
        TEST    al,80h                          ; Do colors match ?
        JZ      PA1                             ; Yes - jump
        MOV     al,1                            ; Set different color flag
        RET                                     ; Return
PA1:    MOV     al,2                            ; Set same color flag
        RET                                     ; Return
PA2:    MOV     al,3                            ; Set off board flag
        RET                                     ; Return

;***********************************************************
; PIECE MOVER ROUTINE
;***********************************************************
; FUNCTION:   To generate all the possible legal moves for a
;             given piece.
;
; CALLED BY:  GENMOV
;
; CALLS:      PATH
;             ADMOVE
;             CASTLE
;             ENPSNT
;
; ARGUMENTS:  The piece to be moved.
;***********************************************************
MPIECE: XOR     al,byte ptr [ebp+ebx]           ; Piece to move
        AND     al,87H                          ; Clear flag bit
        CMP     al,BPAWN                        ; Is it a black Pawn ?
        JNZ     rel001                          ; No-Skip
        DEC     al                              ; Decrement for black Pawns
rel001: AND     al,7                            ; Get piece type
        MOV     byte ptr [ebp+T1],al            ; Save piece type
        MOV     di,word ptr [ebp+T1]            ; Load index to DCOUNT/DPOINT
        MOV     ch,byte ptr [ebp+edi+DCOUNT]    ; Get direction count
        MOV     al,byte ptr [ebp+edi+DPOINT]    ; Get direction pointer
        MOV     byte ptr [ebp+INDX2],al         ; Save as index to direct
        MOV     di,word ptr [ebp+INDX2]         ; Load index
MP5:    MOV     cl,byte ptr [ebp+edi+DIRECT]    ; Get move direction
        MOV     al,byte ptr [ebp+M1]            ; From position
        MOV     byte ptr [ebp+M2],al            ; Initialize to position
MP10:   CALL    PATH                            ; Calculate next position
        CALLBACK "Suppress King moves"
        CMP     al,2                            ; Ready for new direction ?
        JNC     MP15                            ; Yes - Jump
        AND     al,al                           ; Test for empty square
        Z80_EXAF                                ; Save result
        MOV     al,byte ptr [ebp+T1]            ; Get piece moved
        CMP     al,PAWN+1                       ; Is it a Pawn ?
        JC      MP20                            ; Yes - Jump
        CALL    ADMOVE                          ; Add move to list
        Z80_EXAF                                ; Empty square ?
        JNZ     MP15                            ; No - Jump
        MOV     al,byte ptr [ebp+T1]            ; Piece type
        CMP     al,KING                         ; King ?
        JZ      MP15                            ; Yes - Jump
        CMP     al,BISHOP                       ; Bishop, Rook, or Queen ?
        JNC     MP10                            ; Yes - Jump
MP15:   LAHF                                    ; Increment direction index
        INC     di
        SAHF
        LAHF                                    ; Decr. count-jump if non-zerc
        DEC     ch
        JNZ     MP5
        SAHF
        MOV     al,byte ptr [ebp+T1]            ; Piece type
        CMP     al,KING                         ; King ?
        JNZ     skip2                           ; Yes - Try Castling
        CALL    CASTLE
skip2:
        RET                                     ; Return
; ***** PAWN LOGIC *****
MP20:   MOV     al,ch                           ; Counter for direction
        CMP     al,3                            ; On diagonal moves ?
        JC      MP35                            ; Yes - Jump
        JZ      MP30                            ; -or-jump if on 2 square move
        Z80_EXAF                                ; Is forward square empty?
        JNZ     MP15                            ; No - jump
        MOV     al,byte ptr [ebp+M2]            ; Get "to" position
        CMP     al,91                           ; Promote white Pawn ?
        JNC     MP25                            ; Yes - Jump
        CMP     al,29                           ; Promote black Pawn ?
        JNC     MP26                            ; No - Jump
MP25:   MOV     bx,P2                           ; Flag address
        LAHF                                    ; Set promote flag
        OR      byte ptr [ebp+ebx],20h
        SAHF
MP26:   CALL    ADMOVE                          ; Add to move list
        LAHF                                    ; Adjust to two square move
        INC     di
        SAHF
        DEC     ch
        MOV     bx,P1                           ; Check Pawn moved flag
        TEST    byte ptr [ebp+ebx],8            ; Has it moved before ?
        JZ      MP10                            ; No - Jump
        JMP     MP15                            ; Jump
MP30:   Z80_EXAF                                ; Is forward square empty ?
        JNZ     MP15                            ; No - Jump
MP31:   CALL    ADMOVE                          ; Add to move list
        JMP     MP15                            ; Jump
MP35:   Z80_EXAF                                ; Is diagonal square empty ?
        JZ      MP36                            ; Yes - Jump
        MOV     al,byte ptr [ebp+M2]            ; Get "to" position
        CMP     al,91                           ; Promote white Pawn ?
        JNC     MP37                            ; Yes - Jump
        CMP     al,29                           ; Black Pawn promotion ?
        JNC     MP31                            ; No- Jump
MP37:   MOV     bx,P2                           ; Get flag address
        LAHF                                    ; Set promote flag
        OR      byte ptr [ebp+ebx],20h
        SAHF
        JMP     MP31                            ; Jump
MP36:   CALL    ENPSNT                          ; Try en passant capture
        JMP     MP15                            ; Jump

;***********************************************************
; EN PASSANT ROUTINE
;***********************************************************
; FUNCTION:   --  To test for en passant Pawn capture and
;                 to add it to the move list if it is
;                 legal.
;
; CALLED BY:  --  MPIECE
;
; CALLS:      --  ADMOVE
;                 ADJPTR
;
; ARGUMENTS:  --  None
;***********************************************************
ENPSNT: MOV     al,byte ptr [ebp+M1]            ; Set position of Pawn
        MOV     bx,P1                           ; Check color
        TEST    byte ptr [ebp+ebx],80h          ; Is it white ?
        JZ      rel002                          ; Yes - skip
        ADD     al,10                           ; Add 10 for black
rel002: CMP     al,61                           ; On en passant capture rank ?
        JNC     skip3                           ; No - return
        RET
skip3:
        CMP     al,69                           ; On en passant capture rank ?
        JC      skip4                           ; No - return
        RET
skip4:
        MOV     si,word ptr [ebp+MLPTRJ]        ; Get pointer to previous move
        TEST    byte ptr [ebp+esi+MLFLG],10h    ; First move for that piece ?
        JNZ     skip5                           ; No - return
        RET
skip5:
        MOV     al,byte ptr [ebp+esi+MLTOP]     ; Get "to" position
        MOV     byte ptr [ebp+M4],al            ; Store as index to board
        MOV     si,word ptr [ebp+M4]            ; Load board index
        MOV     al,byte ptr [ebp+esi+BOARD]     ; Get piece moved
        MOV     byte ptr [ebp+P3],al            ; Save it
        AND     al,7                            ; Get piece type
        CMP     al,PAWN                         ; Is it a Pawn ?
        JZ      skip6                           ; No - return
        RET
skip6:
        MOV     al,byte ptr [ebp+M4]            ; Get "to" position
        MOV     bx,M2                           ; Get present "to" position
        SUB     al,byte ptr [ebp+ebx]           ; Find difference
        JNS     rel003                          ; Positive ? Yes - Jump
        NEG     al                              ; Else take absolute value
rel003: CMP     al,10                           ; Is difference 10 ?
        JZ      skip7                           ; No - return
        RET
skip7:
        MOV     bx,P2                           ; Address of flags
        LAHF                                    ; Set double move flag
        OR      byte ptr [ebp+ebx],40h
        SAHF
        CALL    ADMOVE                          ; Add Pawn move to move list
        MOV     al,byte ptr [ebp+M1]            ; Save initial Pawn position
        MOV     byte ptr [ebp+M3],al
        MOV     al,byte ptr [ebp+M4]            ; Set "from" and "to" positions
                                                ; for dummy move
        MOV     byte ptr [ebp+M1],al
        MOV     byte ptr [ebp+M2],al
        MOV     al,byte ptr [ebp+P3]            ; Save captured Pawn
        MOV     byte ptr [ebp+P2],al
        CALL    ADMOVE                          ; Add Pawn capture to move list
        MOV     al,byte ptr [ebp+M3]            ; Restore "from" position
        MOV     byte ptr [ebp+M1],al

;***********************************************************
; ADJUST MOVE LIST POINTER FOR DOUBLE MOVE
;***********************************************************
; FUNCTION:   --  To adjust move list pointer to link around
;                 second move in double move.
;
; CALLED BY:  --  ENPSNT
;                 CASTLE
;                 (This mini-routine is not really called,
;                 but is jumped to to save time.)
;
; CALLS:      --  None
;
; ARGUMENTS:  --  None
;***********************************************************
ADJPTR: MOV     bx,word ptr [ebp+MLLST]         ; Get list pointer
        MOV     dx,-6                           ; Size of a move entry
        LAHF                                    ; Back up list pointer
        ADD     bx,dx
        SAHF
        MOV     word ptr [ebp+MLLST],bx         ; Save list pointer
        MOV     byte ptr [ebp+ebx],0            ; Zero out link, first byte
        LAHF                                    ; Next byte
        INC     bx
        SAHF
        MOV     byte ptr [ebp+ebx],0            ; Zero out link, second byte
        RET                                     ; Return

;***********************************************************
; CASTLE ROUTINE
;***********************************************************
; FUNCTION:   --  To determine whether castling is legal
;                 (Queen side, King side, or both) and add it
;                 to the move list if it is.
;
; CALLED BY:  --  MPIECE
;
; CALLS:      --  ATTACK
;                 ADMOVE
;                 ADJPTR
;
; ARGUMENTS:  --  None
;***********************************************************
CASTLE: MOV     al,byte ptr [ebp+P1]            ; Get King
        TEST    al,8                            ; Has it moved ?
        JZ      skip8                           ; Yes - return
        RET
skip8:
        MOV     al,byte ptr [ebp+CKFLG]         ; Fetch Check Flag
        AND     al,al                           ; Is the King in check ?
        JZ      skip9                           ; Yes - Return
        RET
skip9:
        MOV     cx,0FF03H                       ; Initialize King-side values
CA5:    MOV     al,byte ptr [ebp+M1]            ; King position
        ADD     al,cl                           ; Rook position
        MOV     cl,al                           ; Save
        MOV     byte ptr [ebp+M3],al            ; Store as board index
        MOV     si,word ptr [ebp+M3]            ; Load board index
        MOV     al,byte ptr [ebp+esi+BOARD]     ; Get contents of board
        AND     al,7FH                          ; Clear color bit
        CMP     al,ROOK                         ; Has Rook ever moved ?
        JNZ     CA20                            ; Yes - Jump
        MOV     al,cl                           ; Restore Rook position
        JMP     CA15                            ; Jump
CA10:   MOV     si,word ptr [ebp+M3]            ; Load board index
        MOV     al,byte ptr [ebp+esi+BOARD]     ; Get contents of board
        AND     al,al                           ; Empty ?
        JNZ     CA20                            ; No - Jump
        MOV     al,byte ptr [ebp+M3]            ; Current position
        CMP     al,22                           ; White Queen Knight square ?
        JZ      CA15                            ; Yes - Jump
        CMP     al,92                           ; Black Queen Knight square ?
        JZ      CA15                            ; Yes - Jump
        CALL    ATTACK                          ; Look for attack on square
        AND     al,al                           ; Any attackers ?
        JNZ     CA20                            ; Yes - Jump
        MOV     al,byte ptr [ebp+M3]            ; Current position
CA15:   ADD     al,ch                           ; Next position
        MOV     byte ptr [ebp+M3],al            ; Save as board index
        MOV     bx,M1                           ; King position
        CMP     al,byte ptr [ebp+ebx]           ; Reached King ?
        JNZ     CA10                            ; No - jump
        SUB     al,ch                           ; Determine King's position
        SUB     al,ch
        MOV     byte ptr [ebp+M2],al            ; Save it
        MOV     bx,P2                           ; Address of flags
        MOV     byte ptr [ebp+ebx],40H          ; Set double move flag
        CALL    ADMOVE                          ; Put king move in list
        MOV     bx,M1                           ; Addr of King "from" position
        MOV     al,byte ptr [ebp+ebx]           ; Get King's "from" position
        MOV     byte ptr [ebp+ebx],cl           ; Store Rook "from" position
        SUB     al,ch                           ; Get Rook "to" position
        MOV     byte ptr [ebp+M2],al            ; Store Rook "to" position
        XOR     al,al                           ; Zero
        MOV     byte ptr [ebp+P2],al            ; Zero move flags
        CALL    ADMOVE                          ; Put Rook move in list
        CALL    ADJPTR                          ; Re-adjust move list pointer
        MOV     al,byte ptr [ebp+M3]            ; Restore King position
        MOV     byte ptr [ebp+M1],al            ; Store
CA20:   MOV     al,ch                           ; Scan Index
        CMP     al,1                            ; Done ?
        JNZ     skip10                          ; Yes - return
        RET
skip10:
        MOV     cx,01FCH                        ; Set Queen-side initial values
        JMP     CA5                             ; Jump

;***********************************************************
; ADMOVE ROUTINE
;***********************************************************
; FUNCTION:   --  To add a move to the move list
;
; CALLED BY:  --  MPIECE
;                 ENPSNT
;                 CASTLE
;
; CALLS:      --  None
;
; ARGUMENT:  --  None
;***********************************************************
ADMOVE: MOV     dx,word ptr [ebp+MLNXT]         ; Addr of next loc in move list
        MOV     bx,MLEND                        ; Address of list end
        AND     al,al                           ; Clear carry flag
        SBB     bx,dx                           ; Calculate difference
        JC      AM10                            ; Jump if out of space
        MOV     bx,word ptr [ebp+MLLST]         ; Addr of prev. list area
        MOV     word ptr [ebp+MLLST],dx         ; Save next as previous
        MOV     byte ptr [ebp+ebx],dl           ; Store link address
        LAHF
        INC     bx
        SAHF
        MOV     byte ptr [ebp+ebx],dh
        MOV     bx,P1                           ; Address of moved piece
        TEST    byte ptr [ebp+ebx],8            ; Has it moved before ?
        JNZ     rel004                          ; Yes - jump
        MOV     bx,P2                           ; Address of move flags
        LAHF                                    ; Set first move flag
        OR      byte ptr [ebp+ebx],10h
        SAHF
rel004: XCHG    bx,dx                           ; Address of move area
        MOV     byte ptr [ebp+ebx],0            ; Store zero in link address
        LAHF
        INC     bx
        SAHF
        MOV     byte ptr [ebp+ebx],0
        LAHF
        INC     bx
        SAHF
        MOV     al,byte ptr [ebp+M1]            ; Store "from" move position
        MOV     byte ptr [ebp+ebx],al
        LAHF
        INC     bx
        SAHF
        MOV     al,byte ptr [ebp+M2]            ; Store "to" move position
        MOV     byte ptr [ebp+ebx],al
        LAHF
        INC     bx
        SAHF
        MOV     al,byte ptr [ebp+P2]            ; Store move flags/capt. piece
        MOV     byte ptr [ebp+ebx],al
        LAHF
        INC     bx
        SAHF
        MOV     byte ptr [ebp+ebx],0            ; Store initial move value
        LAHF
        INC     bx
        SAHF
        MOV     word ptr [ebp+MLNXT],bx         ; Save address for next move
        RET                                     ; Return
AM10:   MOV     byte ptr [ebp+ebx],0            ; Abort entry on table ovflow
        LAHF
        INC     bx
        SAHF
        MOV     byte ptr [ebp+ebx],0            ; TODO fix this or at least look at it
        LAHF
        DEC     bx
        SAHF
        RET

;***********************************************************
; GENERATE MOVE ROUTINE
;***********************************************************
; FUNCTION:  --  To generate the move set for all of the
;                pieces of a given color.
;
; CALLED BY: --  FNDMOV
;
; CALLS:     --  MPIECE
;                INCHK
;
; ARGUMENTS: --  None
;***********************************************************
GENMOV: CALL    INCHK                           ; Test for King in check
        MOV     byte ptr [ebp+CKFLG],al         ; Save attack count as flag
        MOV     dx,word ptr [ebp+MLNXT]         ; Addr of next avail list space
        MOV     bx,word ptr [ebp+MLPTRI]        ; Ply list pointer index
        LAHF                                    ; Increment to next ply
        INC     bx
        SAHF
        LAHF
        INC     bx
        SAHF
        MOV     byte ptr [ebp+ebx],dl           ; Save move list pointer
        LAHF
        INC     bx
        SAHF
        MOV     byte ptr [ebp+ebx],dh
        LAHF
        INC     bx
        SAHF
        MOV     word ptr [ebp+MLPTRI],bx        ; Save new index
        MOV     word ptr [ebp+MLLST],bx         ; Last pointer for chain init.
        MOV     al,21                           ; First position on board
GM5:    MOV     byte ptr [ebp+M1],al            ; Save as index
        MOV     si,word ptr [ebp+M1]            ; Load board index
        MOV     al,byte ptr [ebp+esi+BOARD]     ; Fetch board contents
        AND     al,al                           ; Is it empty ?
        JZ      GM10                            ; Yes - Jump
        CMP     al,-1                           ; Is it a border square ?
        JZ      GM10                            ; Yes - Jump
        MOV     byte ptr [ebp+P1],al            ; Save piece
        MOV     bx,COLOR                        ; Address of color of piece
        XOR     al,byte ptr [ebp+ebx]           ; Test color of piece
        TEST    al,80h                          ; Match ?
        JNZ     skip11                          ; Yes - call Move Piece
        CALL    MPIECE
skip11:
GM10:   MOV     al,byte ptr [ebp+M1]            ; Fetch current board position
        INC     al                              ; Incr to next board position
        CMP     al,99                           ; End of board array ?
        JNZ     GM5                             ; No - Jump
        RET                                     ; Return

;***********************************************************
; CHECK ROUTINE
;***********************************************************
; FUNCTION:   --  To determine whether or not the
;                 King is in check.
;
; CALLED BY:  --  GENMOV
;                 FNDMOV
;                 EVAL
;
; CALLS:      --  ATTACK
;
; ARGUMENTS:  --  Color of King
;***********************************************************
INCHK:  MOV     al,byte ptr [ebp+COLOR]         ; Get color
INCHK1: MOV     bx,POSK                         ; Addr of white King position
        AND     al,al                           ; White ?
        JZ      rel005                          ; Yes - Skip
        LAHF                                    ; Addr of black King position
        INC     bx
        SAHF
rel005: MOV     al,byte ptr [ebp+ebx]           ; Fetch King position
        MOV     byte ptr [ebp+M3],al            ; Save
        MOV     si,word ptr [ebp+M3]            ; Load board index
        MOV     al,byte ptr [ebp+esi+BOARD]     ; Fetch board contents
        MOV     byte ptr [ebp+P1],al            ; Save
        AND     al,7                            ; Get piece type
        MOV     byte ptr [ebp+T1],al            ; Save
        CALL    ATTACK                          ; Look for attackers on King
        RET                                     ; Return

;***********************************************************
; ATTACK ROUTINE
;***********************************************************
; FUNCTION:   --  To find all attackers on a given square
;                 by scanning outward from the square
;                 until a piece is found that attacks
;                 that square, or a piece is found that
;                 doesn't attack that square, or the edge
;                 of the board is reached.
;
;                 In determining which pieces attack
;                 a square, this routine also takes into
;                 account the ability of certain pieces to
;                 attack through another attacking piece. (For
;                 example a queen lined up behind a bishop
;                 of her same color along a diagonal.) The
;                 bishop is then said to be transparent to the
;                 queen, since both participate in the
;                 attack.
;
;                 In the case where this routine is called
;                 by CASTLE or INCHK, the routine is
;                 terminated as soon as an attacker of the
;                 opposite color is encountered.
;
; CALLED BY:  --  POINTS
;                 PINFND
;                 CASTLE
;                 INCHK
;
; CALLS:      --  PATH
;                 ATKSAV
;
; ARGUMENTS:  --  None
;***********************************************************
ATTACK: PUSH    ecx                             ; Save Register B
        XOR     al,al                           ; Clear
        MOV     ch,16                           ; Initial direction count
        MOV     byte ptr [ebp+INDX2],al         ; Initial direction index
        MOV     di,word ptr [ebp+INDX2]         ; Load index
AT5:    MOV     cl,byte ptr [ebp+edi+DIRECT]    ; Get direction
        MOV     dh,0                            ; Init. scan count/flags
        MOV     al,byte ptr [ebp+M3]            ; Init. board start position
        MOV     byte ptr [ebp+M2],al            ; Save
AT10:   INC     dh                              ; Increment scan count
        CALL    PATH                            ; Next position
        CMP     al,1                            ; Piece of a opposite color ?
        JZ      AT14A                           ; Yes - jump
        CMP     al,2                            ; Piece of same color ?
        JZ      AT14B                           ; Yes - jump
        AND     al,al                           ; Empty position ?
        JNZ     AT12                            ; No - jump
        MOV     al,ch                           ; Fetch direction count
        CMP     al,9                            ; On knight scan ?
        JNC     AT10                            ; No - jump
AT12:   LAHF                                    ; Increment direction index
        INC     di
        SAHF
        LAHF                                    ; Done ? No - jump
        DEC     ch
        JNZ     AT5
        SAHF
        XOR     al,al                           ; No attackers
AT13:   POP     ecx                             ; Restore register B
        RET                                     ; Return
AT14A:  TEST    dh,40h                          ; Same color found already ?
        JNZ     AT12                            ; Yes - jump
        LAHF                                    ; Set opposite color found flag
        OR      dh,20h
        SAHF
        JMP     AT14                            ; Jump
AT14B:  TEST    dh,20h                          ; Opposite color found already?
        JNZ     AT12                            ; Yes - jump
        LAHF                                    ; Set same color found flag
        OR      dh,40h
        SAHF

;
; ***** DETERMINE IF PIECE ENCOUNTERED ATTACKS SQUARE *****
AT14:   MOV     al,byte ptr [ebp+T2]            ; Fetch piece type encountered
        MOV     dl,al                           ; Save
        MOV     al,ch                           ; Get direction-counter
        CMP     al,9                            ; Look for Knights ?
        JC      AT25                            ; Yes - jump
        MOV     al,dl                           ; Get piece type
        CMP     al,QUEEN                        ; Is is a Queen ?
        JNZ     AT15                            ; No - Jump
        LAHF                                    ; Set Queen found flag
        OR      dh,80h
        SAHF
        JMP     AT30                            ; Jump
AT15:   MOV     al,dh                           ; Get flag/scan count
        AND     al,0FH                          ; Isolate count
        CMP     al,1                            ; On first position ?
        JNZ     AT16                            ; No - jump
        MOV     al,dl                           ; Get encountered piece type
        CMP     al,KING                         ; Is it a King ?
        JZ      AT30                            ; Yes - jump
AT16:   MOV     al,ch                           ; Get direction counter
        CMP     al,13                           ; Scanning files or ranks ?
        JC      AT21                            ; Yes - jump
        MOV     al,dl                           ; Get piece type
        CMP     al,BISHOP                       ; Is it a Bishop ?
        JZ      AT30                            ; Yes - jump
        MOV     al,dh                           ; Get flags/scan count
        AND     al,0FH                          ; Isolate count
        CMP     al,1                            ; On first position ?
        JNZ     AT12                            ; No - jump
        CMP     al,dl                           ; Is it a Pawn ?
        JNZ     AT12                            ; No - jump
        MOV     al,byte ptr [ebp+P2]            ; Fetch piece including color
        TEST    al,80h                          ; Is it white ?
        JZ      AT20                            ; Yes - jump
        MOV     al,ch                           ; Get direction counter
        CMP     al,15                           ; On a non-attacking diagonal ?
        JC      AT12                            ; Yes - jump
        JMP     AT30                            ; Jump
AT20:   MOV     al,ch                           ; Get direction counter
        CMP     al,15                           ; On a non-attacking diagonal ?
        JNC     AT12                            ; Yes - jump
        JMP     AT30                            ; Jump
AT21:   MOV     al,dl                           ; Get piece type
        CMP     al,ROOK                         ; Is is a Rook ?
        JNZ     AT12                            ; No - jump
        JMP     AT30                            ; Jump
AT25:   MOV     al,dl                           ; Get piece type
        CMP     al,KNIGHT                       ; Is it a Knight ?
        JNZ     AT12                            ; No - jump
AT30:   MOV     al,byte ptr [ebp+T1]            ; Attacked piece type/flag
        CMP     al,7                            ; Call from POINTS ?
        JZ      AT31                            ; Yes - jump
        TEST    dh,20h                          ; Is attacker opposite color ?
        JZ      AT32                            ; No - jump
        MOV     al,1                            ; Set attacker found flag
        JMP     AT13                            ; Jump
AT31:   CALL    ATKSAV                          ; Save attacker in attack list
AT32:   MOV     al,byte ptr [ebp+T2]            ; Attacking piece type
        CMP     al,KING                         ; Is it a King,?
        JZ      AT12                            ; Yes - jump
        CMP     al,KNIGHT                       ; Is it a Knight ?
        JZ      AT12                            ; Yes - jump
        JMP     AT10                            ; Jump

;***********************************************************
; ATTACK SAVE ROUTINE
;***********************************************************
; FUNCTION:   --  To save an attacking piece value in the
;                 attack list, and to increment the attack
;                 count for that color piece.
;
;                 The pin piece list is checked for the
;                 attacking piece, and if found there, the
;                 piece is not included in the attack list.
;
; CALLED BY:  --  ATTACK
;
; CALLS:      --  PNCK
;
; ARGUMENTS:  --  None
;***********************************************************
ATKSAV: PUSH    ecx                             ; Save Regs BC
        PUSH    edx                             ; Save Regs DE
        MOV     al,byte ptr [ebp+NPINS]         ; Number of pinned pieces
        AND     al,al                           ; Any ?
        JZ      skip12                          ; yes - check pin list
        CALL    PNCK
skip12:
        MOV     si,word ptr [ebp+T2]            ; Init index to value table
        MOV     bx,ATKLST                       ; Init address of attack list
        MOV     cx,0                            ; Init increment for white
        MOV     al,byte ptr [ebp+P2]            ; Attacking piece
        TEST    al,80h                          ; Is it white ?
        JZ      rel006                          ; Yes - jump
        MOV     cl,7                            ; Init increment for black
rel006: AND     al,7                            ; Attacking piece type
        MOV     dl,al                           ; Init increment for type
        TEST    dh,80h                          ; Queen found this scan ?
        JZ      rel007                          ; No - jump
        MOV     dl,QUEEN                        ; Use Queen slot in attack list
rel007: LAHF                                    ; Attack list address
        ADD     bx,cx
        SAHF
        INC     byte ptr [ebp+ebx]              ; Increment list count
        MOV     dh,0
        LAHF                                    ; Attack list slot address
        ADD     bx,dx
        SAHF
        MOV     al,byte ptr [ebp+ebx]           ; Get data already there
        AND     al,0FH                          ; Is first slot empty ?
        JZ      AS20                            ; Yes - jump
        MOV     al,byte ptr [ebp+ebx]           ; Get data again
        AND     al,0F0H                         ; Is second slot empty ?
        JZ      AS19                            ; Yes - jump
        LAHF                                    ; Increment to King slot
        INC     bx
        SAHF
        JMP     AS20                            ; Jump
AS19:   Z80_RLD                                 ; Temp save lower in upper
        MOV     al,byte ptr [ebp+esi+PVALUE]    ; Get new value for attack list
        Z80_RRD                                 ; Put in 2nd attack list slot
        JMP     AS25                            ; Jump
AS20:   MOV     al,byte ptr [ebp+esi+PVALUE]    ; Get new value for attack list
        Z80_RLD                                 ; Put in 1st attack list slot
AS25:   POP     edx                             ; Restore DE regs
        POP     ecx                             ; Restore BC regs
        RET                                     ; Return

;***********************************************************
; PIN CHECK ROUTINE
;***********************************************************
; FUNCTION:   --  Checks to see if the attacker is in the
;                 pinned piece list. If so he is not a valid
;                 attacker unless the direction in which he
;                 attacks is the same as the direction along
;                 which he is pinned. If the piece is
;                 found to be invalid as an attacker, the
;                 return to the calling routine is aborted
;                 and this routine returns directly to ATTACK.
;
; CALLED BY:  --  ATKSAV
;
; CALLS:      --  None
;
; ARGUMENTS:  --  The direction of the attack. The
;                 pinned piece counnt.
;***********************************************************
PNCK:   MOV     dh,cl                           ; Save attack direction
        MOV     dl,0                            ; Clear flag
        MOV     cl,al                           ; Load pin count for search
        MOV     ch,0
        MOV     al,byte ptr [ebp+M2]            ; Position of piece
        MOV     bx,PLISTA                       ; Pin list address
PC1:    Z80_CPIR                                ; Search list for position
        JZ      skip13                          ; Return if not found
        RET
skip13:
        Z80_EXAF                                ; Save search paramenters
        TEST    dl,1                            ; Is this the first find ?
        JNZ     PC5                             ; No - jump
        LAHF                                    ; Set first find flag
        OR      dl,1
        SAHF
        PUSH    ebx                             ; Get corresp index to dir list
        POP     esi
        MOV     al,byte ptr [ebp+esi+9]         ; Get direction
        CMP     al,dh                           ; Same as attacking direction ?
        JZ      PC3                             ; Yes - jump
        NEG     al                              ; Opposite direction ?
        CMP     al,dh                           ; Same as attacking direction ?
        JNZ     PC5                             ; No - jump
PC3:    Z80_EXAF                                ; Restore search parameters
        JPE     PC1                             ; Jump if search not complete
        RET                                     ; Return
PC5:    POP     eax                             ; Abnormal exit
        SAHF
        POP     edx                             ; Restore regs.
        POP     ecx
        RET                                     ; Return to ATTACK

;***********************************************************
; PIN FIND ROUTINE
;***********************************************************
; FUNCTION:   --  To produce a list of all pieces pinned
;                 against the King or Queen, for both white
;                 and black.
;
; CALLED BY:  --  FNDMOV
;                 EVAL
;
; CALLS:      --  PATH
;                 ATTACK
;
; ARGUMENTS:  --  None
;***********************************************************
PINFND: XOR     al,al                           ; Zero pin count
        MOV     byte ptr [ebp+NPINS],al
        MOV     dx,POSK                         ; Addr of King/Queen pos list
PF1:    MOV     al,byte ptr [ebp+edx]           ; Get position of royal piece
        AND     al,al                           ; Is it on board ?
        JZ      PF26                            ; No- jump
        CMP     al,-1                           ; At end of list ?
        JNZ     skip14                          ; Yes return
        RET
skip14:
        MOV     byte ptr [ebp+M3],al            ; Save position as board index
        MOV     si,word ptr [ebp+M3]            ; Load index to board
        MOV     al,byte ptr [ebp+esi+BOARD]     ; Get contents of board
        MOV     byte ptr [ebp+P1],al            ; Save
        MOV     ch,8                            ; Init scan direction count
        XOR     al,al
        MOV     byte ptr [ebp+INDX2],al         ; Init direction index
        MOV     di,word ptr [ebp+INDX2]
PF2:    MOV     al,byte ptr [ebp+M3]            ; Get King/Queen position
        MOV     byte ptr [ebp+M2],al            ; Save
        XOR     al,al
        MOV     byte ptr [ebp+M4],al            ; Clear pinned piece saved pos
        MOV     cl,byte ptr [ebp+edi+DIRECT]    ; Get direction of scan
PF5:    CALL    PATH                            ; Compute next position
        AND     al,al                           ; Is it empty ?
        JZ      PF5                             ; Yes - jump
        CMP     al,3                            ; Off board ?
        JZ      PF25                            ; Yes - jump
        CMP     al,2                            ; Piece of same color
        MOV     al,byte ptr [ebp+M4]            ; Load pinned piece position
        JZ      PF15                            ; Yes - jump
        AND     al,al                           ; Possible pin ?
        JZ      PF25                            ; No - jump
        MOV     al,byte ptr [ebp+T2]            ; Piece type encountered
        CMP     al,QUEEN                        ; Queen ?
        JZ      PF19                            ; Yes - jump
        MOV     bl,al                           ; Save piece type
        MOV     al,ch                           ; Direction counter
        CMP     al,5                            ; Non-diagonal direction ?
        JC      PF10                            ; Yes - jump
        MOV     al,bl                           ; Piece type
        CMP     al,BISHOP                       ; Bishop ?
        JNZ     PF25                            ; No - jump
        JMP     PF20                            ; Jump
PF10:   MOV     al,bl                           ; Piece type
        CMP     al,ROOK                         ; Rook ?
        JNZ     PF25                            ; No - jump
        JMP     PF20                            ; Jump
PF15:   AND     al,al                           ; Possible pin ?
        JNZ     PF25                            ; No - jump
        MOV     al,byte ptr [ebp+M2]            ; Save possible pin position
        MOV     byte ptr [ebp+M4],al
        JMP     PF5                             ; Jump
PF19:   MOV     al,byte ptr [ebp+P1]            ; Load King or Queen
        AND     al,7                            ; Clear flags
        CMP     al,QUEEN                        ; Queen ?
        JNZ     PF20                            ; No - jump
        PUSH    ecx                             ; Save regs.
        PUSH    edx
        PUSH    edi
        XOR     al,al                           ; Zero out attack list
        MOV     ch,14
        MOV     bx,ATKLST
back02: MOV     byte ptr [ebp+ebx],al
        LAHF
        INC     bx
        SAHF
        LAHF
        DEC     ch
        JNZ     back02
        SAHF
        MOV     al,7                            ; Set attack flag
        MOV     byte ptr [ebp+T1],al
        CALL    ATTACK                          ; Find attackers/defenders
        MOV     bx,WACT                         ; White queen attackers
        MOV     dx,BACT                         ; Black queen attackers
        MOV     al,byte ptr [ebp+P1]            ; Get queen
        TEST    al,80h                          ; Is she white ?
        JZ      rel008                          ; Yes - skip
        XCHG    bx,dx                           ; Reverse for black
rel008: MOV     al,byte ptr [ebp+ebx]           ; Number of defenders
        XCHG    bx,dx                           ; Reverse for attackers
        SUB     al,byte ptr [ebp+ebx]           ; Defenders minus attackers
        DEC     al                              ; Less 1
        POP     edi                             ; Restore regs.
        POP     edx
        POP     ecx
        JNS     PF25                            ; Jump if pin not valid
PF20:   MOV     bx,NPINS                        ; Address of pinned piece count
        INC     byte ptr [ebp+ebx]              ; Increment
        MOV     si,word ptr [ebp+NPINS]         ; Load pin list index
        MOV     byte ptr [ebp+esi+PLISTD],cl    ; Save direction of pin
        MOV     al,byte ptr [ebp+M4]            ; Position of pinned piece
        MOV     byte ptr [ebp+esi+PLIST],al     ; Save in list
PF25:   LAHF                                    ; Increment direction index
        INC     di
        SAHF
        LAHF                                    ; Done ? No - Jump
        DEC     ch
        JNZ     PF27
        SAHF
PF26:   LAHF                                    ; Incr King/Queen pos index
        INC     dx
        SAHF
        JMP     PF1                             ; Jump
PF27:   JMP     PF2                             ; Jump

;***********************************************************
; EXCHANGE ROUTINE
;***********************************************************
; FUNCTION:   --  To determine the exchange value of a
;                 piece on a given square by examining all
;                 attackers and defenders of that piece.
;
; CALLED BY:  --  POINTS
;
; CALLS:      --  NEXTAD
;
; ARGUMENTS:  --  None.
;***********************************************************
XCHNG:  Z80_EXX                                 ; Swap regs.
        MOV     al,byte ptr [ebp+P1]            ; Piece attacked
        MOV     bx,WACT                         ; Addr of white attkrs/dfndrs
        MOV     dx,BACT                         ; Addr of black attkrs/dfndrs
        TEST    al,80h                          ; Is piece white ?
        JZ      rel009                          ; Yes - jump
        XCHG    bx,dx                           ; Swap list pointers
rel009: MOV     ch,byte ptr [ebp+ebx]           ; Init list counts
        XCHG    bx,dx
        MOV     cl,byte ptr [ebp+ebx]
        XCHG    bx,dx
        Z80_EXX                                 ; Restore regs.
        MOV     cl,0                            ; Init attacker/defender flag
        MOV     dl,0                            ; Init points lost count
        MOV     si,word ptr [ebp+T3]            ; Load piece value index
        MOV     dh,byte ptr [ebp+esi+PVALUE]    ; Get attacked piece value
        SHL     dh,1                            ; Double it
        MOV     ch,dh                           ; Save
        CALL    NEXTAD                          ; Retrieve first attacker
        JNZ     skip15                          ; Return if none
        RET
skip15:
XC10:   MOV     bl,al                           ; Save attacker value
        CALL    NEXTAD                          ; Get next defender
        JZ      XC18                            ; Jump if none
        Z80_EXAF                                ; Save defender value
        MOV     al,ch                           ; Get attacked value
        CMP     al,bl                           ; Attacked less than attacker ?
        JNC     XC19                            ; No - jump
        Z80_EXAF                                ; -Restore defender
XC15:   CMP     al,bl                           ; Defender less than attacker ?
        JNC     skip16                          ; Yes - return
        RET
skip16:
        CALL    NEXTAD                          ; Retrieve next attacker value
        JNZ     skip17                          ; Return if none
        RET
skip17:
        MOV     bl,al                           ; Save attacker value
        CALL    NEXTAD                          ; Retrieve next defender value
        JNZ     XC15                            ; Jump if none
XC18:   Z80_EXAF                                ; Save Defender
        MOV     al,ch                           ; Get value of attacked piece
XC19:   TEST    cl,1                            ; Attacker or defender ?
        JZ      rel010                          ; Jump if defender
        NEG     al                              ; Negate value for attacker
rel010: ADD     al,dl                           ; Total points lost
        MOV     dl,al                           ; Save total
        Z80_EXAF                                ; Restore previous defender
        JNZ     skip18                          ; Return if none
        RET
skip18:
        MOV     ch,bl                           ; Prev attckr becomes defender
        JMP     XC10                            ; Jump

;***********************************************************
; NEXT ATTACKER/DEFENDER ROUTINE
;***********************************************************
; FUNCTION:   --  To retrieve the next attacker or defender
;                 piece value from the attack list, and delete
;                 that piece from the list.
;
; CALLED BY:  --  XCHNG
;
; CALLS:      --  None
;
; ARGUMENTS:  --  Attack list addresses.
;                 Side flag
;                 Attack list counts
;***********************************************************
NEXTAD: INC     cl                              ; Increment side flag
        Z80_EXX                                 ; Swap registers
        MOV     al,ch                           ; Swap list counts
        MOV     ch,cl
        MOV     cl,al
        XCHG    bx,dx                           ; Swap list pointers
        XOR     al,al
        CMP     al,ch                           ; At end of list ?
        JZ      NX6                             ; Yes - jump
        DEC     ch                              ; Decrement list count
back03: LAHF                                    ; Increment list inter
        INC     bx
        SAHF
        CMP     al,byte ptr [ebp+ebx]           ; Check next item in list
        JZ      back03                          ; Jump if empty
        Z80_RRD                                 ; Get value from list
        ADD     al,al                           ; Double it
        LAHF                                    ; Decrement list pointer
        DEC     bx
        SAHF
NX6:    Z80_EXX                                 ; Restore regs.
        RET                                     ; Return

;***********************************************************
; POINT EVALUATION ROUTINE
;***********************************************************
;FUNCTION:   --  To perform a static board evaluation and
;                derive a score for a given board position
;
; CALLED BY:  --  FNDMOV
;                 EVAL
;
; CALLS:      --  ATTACK
;                 XCHNG
;                 LIMIT
;
; ARGUMENTS:  --  None
;***********************************************************
POINTS: XOR     al,al                           ; Zero out variables
        MOV     byte ptr [ebp+MTRL],al
        MOV     byte ptr [ebp+BRDC],al
        MOV     byte ptr [ebp+PTSL],al
        MOV     byte ptr [ebp+PTSW1],al
        MOV     byte ptr [ebp+PTSW2],al
        MOV     byte ptr [ebp+PTSCK],al
        MOV     bx,T1                           ; Set attacker flag
        MOV     byte ptr [ebp+ebx],7
        MOV     al,21                           ; Init to first square on board
PT5:    MOV     byte ptr [ebp+M3],al            ; Save as board index
        MOV     si,word ptr [ebp+M3]            ; Load board index
        MOV     al,byte ptr [ebp+esi+BOARD]     ; Get piece from board
        CMP     al,-1                           ; Off board edge ?
        JZ      PT25                            ; Yes - jump
        MOV     bx,P1                           ; Save piece, if any
        MOV     byte ptr [ebp+ebx],al
        AND     al,7                            ; Save piece type, if any
        MOV     byte ptr [ebp+T3],al
        CMP     al,KNIGHT                       ; Less than a Knight (Pawn) ?
        JC      PT6X                            ; Yes - Jump
        CMP     al,ROOK                         ; Rook, Queen or King ?
        JC      PT6B                            ; No - jump
        CMP     al,KING                         ; Is it a King ?
        JZ      PT6AA                           ; Yes - jump
        MOV     al,byte ptr [ebp+MOVENO]        ; Get move number
        CMP     al,7                            ; Less than 7 ?
        JC      PT6A                            ; Yes - Jump
        JMP     PT6X                            ; Jump
PT6AA:  TEST    byte ptr [ebp+ebx],10h          ; Castled yet ?
        JZ      PT6A                            ; No - jump
        MOV     al,+6                           ; Bonus for castling
        TEST    byte ptr [ebp+ebx],80h          ; Check piece color
        JZ      PT6D                            ; Jump if white
        MOV     al,-6                           ; Bonus for black castling
        JMP     PT6D                            ; Jump
PT6A:   TEST    byte ptr [ebp+ebx],8            ; Has piece moved yet ?
        JZ      PT6X                            ; No - jump
        JMP     PT6C                            ; Jump
PT6B:   TEST    byte ptr [ebp+ebx],8            ; Has piece moved yet ?
        JNZ     PT6X                            ; Yes - jump
PT6C:   MOV     al,-2                           ; Two point penalty for white
        TEST    byte ptr [ebp+ebx],80h          ; Check piece color
        JZ      PT6D                            ; Jump if white
        MOV     al,+2                           ; Two point penalty for black
PT6D:   MOV     bx,BRDC                         ; Get address of board control
        ADD     al,byte ptr [ebp+ebx]           ; Add on penalty/bonus points
        MOV     byte ptr [ebp+ebx],al           ; Save
PT6X:   XOR     al,al                           ; Zero out attack list
        MOV     ch,14
        MOV     bx,ATKLST
back04: MOV     byte ptr [ebp+ebx],al
        LAHF
        INC     bx
        SAHF
        LAHF
        DEC     ch
        JNZ     back04
        SAHF
        CALL    ATTACK                          ; Build attack list for square
        MOV     bx,BACT                         ; Get black attacker count addr
        MOV     al,byte ptr [ebp+WACT]          ; Get white attacker count
        SUB     al,byte ptr [ebp+ebx]           ; Compute count difference
        MOV     bx,BRDC                         ; Address of board control
        ADD     al,byte ptr [ebp+ebx]           ; Accum board control score
        MOV     byte ptr [ebp+ebx],al           ; Save
        MOV     al,byte ptr [ebp+P1]            ; Get piece on current square
        AND     al,al                           ; Is it empty ?
        JZ      PT25                            ; Yes - jump
        CALL    XCHNG                           ; Evaluate exchange, if any
        XOR     al,al                           ; Check for a loss
        CMP     al,dl                           ; Points lost ?
        JZ      PT23                            ; No - Jump
        DEC     dh                              ; Deduct half a Pawn value
        MOV     al,byte ptr [ebp+P1]            ; Get piece under attack
        MOV     bx,COLOR                        ; Color of side just moved
        XOR     al,byte ptr [ebp+ebx]           ; Compare with piece
        TEST    al,80h                          ; Do colors match ?
        MOV     al,dl                           ; Points lost
        JNZ     PT20                            ; Jump if no match
        MOV     bx,PTSL                         ; Previous max points lost
        CMP     al,byte ptr [ebp+ebx]           ; Compare to current value
        JC      PT23                            ; Jump if greater than
        MOV     byte ptr [ebp+ebx],dl           ; Store new value as max lost
        MOV     si,word ptr [ebp+MLPTRJ]        ; Load pointer to this move
        MOV     al,byte ptr [ebp+M3]            ; Get position of lost piece
        CMP     al,byte ptr [ebp+esi+MLTOP]     ; Is it the one moving ?
        JNZ     PT23                            ; No - jump
        MOV     byte ptr [ebp+PTSCK],al         ; Save position as a flag
        JMP     PT23                            ; Jump
PT20:   MOV     bx,PTSW1                        ; Previous maximum points won
        CMP     al,byte ptr [ebp+ebx]           ; Compare to current value
        JC      rel011                          ; Jump if greater than
        MOV     al,byte ptr [ebp+ebx]           ; Load previous max value
        MOV     byte ptr [ebp+ebx],dl           ; Store new value as max won
rel011: MOV     bx,PTSW2                        ; Previous 2nd max points won
        CMP     al,byte ptr [ebp+ebx]           ; Compare to current value
        JC      PT23                            ; Jump if greater than
        MOV     byte ptr [ebp+ebx],al           ; Store as new 2nd max lost
PT23:   MOV     bx,P1                           ; Get piece
        TEST    byte ptr [ebp+ebx],80h          ; Test color
        MOV     al,dh                           ; Value of piece
        JZ      rel012                          ; Jump if white
        NEG     al                              ; Negate for black
rel012: MOV     bx,MTRL                         ; Get addrs of material total
        ADD     al,byte ptr [ebp+ebx]           ; Add new value
        MOV     byte ptr [ebp+ebx],al           ; Store
PT25:   MOV     al,byte ptr [ebp+M3]            ; Get current board position
        INC     al                              ; Increment
        CMP     al,99                           ; At end of board ?
        JNZ     PT5                             ; No - jump
        MOV     al,byte ptr [ebp+PTSCK]         ; Moving piece lost flag
        AND     al,al                           ; Was it lost ?
        JZ      PT25A                           ; No - jump
        MOV     al,byte ptr [ebp+PTSW2]         ; 2nd max points won
        MOV     byte ptr [ebp+PTSW1],al         ; Store as max points won
        XOR     al,al                           ; Zero out 2nd max points won
        MOV     byte ptr [ebp+PTSW2],al
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
        MOV     bx,MV0                          ; Material at ply 0
        SUB     al,byte ptr [ebp+ebx]           ; Subtract from current
        MOV     ch,al                           ; Save
        MOV     al,30                           ; Load material limit
        CALL    LIMIT                           ; Limit to plus or minus value
        MOV     dl,al                           ; Save limited value
        MOV     al,byte ptr [ebp+BRDC]          ; Get board control points
        MOV     bx,BC0                          ; Board control at ply zero
        SUB     al,byte ptr [ebp+ebx]           ; Get difference
        MOV     ch,al                           ; Save
        MOV     al,byte ptr [ebp+PTSCK]         ; Moving piece lost flag
        AND     al,al                           ; Is it zero ?
        JZ      rel026                          ; Yes - jump
        MOV     ch,0                            ; Zero board control points
rel026: MOV     al,6                            ; Load board control limit
        CALL    LIMIT                           ; Limit to plus or minus value
        MOV     dh,al                           ; Save limited value
        MOV     al,dl                           ; Get material points
        ADD     al,al                           ; Multiply by 4
        ADD     al,al
        ADD     al,dh                           ; Add board control
        MOV     bx,COLOR                        ; Color of side just moved
        TEST    byte ptr [ebp+ebx],80h          ; Is it white ?
        JNZ     rel016                          ; No - jump
        NEG     al                              ; Negate for white
rel016: ADD     al,80H                          ; Rescale score (neutral = 80H)
        CALLBACK "end of POINTS()"
        MOV     byte ptr [ebp+VALM],al          ; Save score
        MOV     si,word ptr [ebp+MLPTRJ]        ; Load move list pointer
        MOV     byte ptr [ebp+esi+MLVAL],al     ; Save score in move list
        RET                                     ; Return

;***********************************************************
; LIMIT ROUTINE
;***********************************************************
; FUNCTION:   --  To limit the magnitude of a given value
;                 to another given value.
;
; CALLED BY:  --  POINTS
;
; CALLS:      --  None
;
; ARGUMENTS:  --  Input  - Value, to be limited in the B
;                          register.
;                        - Value to limit to in the A register
;                 Output - Limited value in the A register.
;***********************************************************
LIMIT:  TEST    ch,80h                          ; Is value negative ?
        JZ      LIM10                           ; No - jump
        NEG     al                              ; Make positive
        CMP     al,ch                           ; Compare to limit
        JC      skip19                          ; Return if outside limit
        RET
skip19:
        MOV     al,ch                           ; Output value as is
        RET                                     ; Return
LIM10:  CMP     al,ch                           ; Compare to limit
        JNC     skip20                          ; Return if outside limit
        RET
skip20:
        MOV     al,ch                           ; Output value as is
        RET                                     ; Return

;***********************************************************
; MOVE ROUTINE
;***********************************************************
; FUNCTION:   --  To execute a move from the move list on the
;                 board array.
;
; CALLED BY:  --  CPTRMV
;                 PLYRMV
;                 EVAL
;                 FNDMOV
;                 VALMOV
;
; CALLS:      --  None
;
; ARGUMENTS:  --  None
;***********************************************************
MOVE:   MOV     bx,word ptr [ebp+MLPTRJ]        ; Load move list pointer
        LAHF                                    ; Increment past link bytes
        INC     bx
        SAHF
        LAHF
        INC     bx
        SAHF
MV1:    MOV     al,byte ptr [ebp+ebx]           ; "From" position
        MOV     byte ptr [ebp+M1],al            ; Save
        LAHF                                    ; Increment pointer
        INC     bx
        SAHF
        MOV     al,byte ptr [ebp+ebx]           ; "To" position
        MOV     byte ptr [ebp+M2],al            ; Save
        LAHF                                    ; Increment pointer
        INC     bx
        SAHF
        MOV     dh,byte ptr [ebp+ebx]           ; Get captured piece/flags
        MOV     si,word ptr [ebp+M1]            ; Load "from" pos board index
        MOV     dl,byte ptr [ebp+esi+BOARD]     ; Get piece moved
        TEST    dh,20h                          ; Test Pawn promotion flag
        JNZ     MV15                            ; Jump if set
        MOV     al,dl                           ; Piece moved
        AND     al,7                            ; Clear flag bits
        CMP     al,QUEEN                        ; Is it a queen ?
        JZ      MV20                            ; Yes - jump
        CMP     al,KING                         ; Is it a king ?
        JZ      MV30                            ; Yes - jump
MV5:    MOV     di,word ptr [ebp+M2]            ; Load "to" pos board index
        LAHF                                    ; Set piece moved flag
        OR      dl,8
        SAHF
        MOV     byte ptr [ebp+edi+BOARD],dl     ; Insert piece at new position
        MOV     byte ptr [ebp+esi+BOARD],0      ; Empty previous position
        TEST    dh,40h                          ; Double move ?
        JNZ     MV40                            ; Yes - jump
        MOV     al,dh                           ; Get captured piece, if any
        AND     al,7
        CMP     al,QUEEN                        ; Was it a queen ?
        JZ      skip21                          ; No - return
        RET
skip21:
        MOV     bx,POSQ                         ; Addr of saved Queen position
        TEST    dh,80h                          ; Is Queen white ?
        JZ      MV10                            ; Yes - jump
        LAHF                                    ; Increment to black Queen pos
        INC     bx
        SAHF
MV10:   XOR     al,al                           ; Set saved position to zero
        MOV     byte ptr [ebp+ebx],al
        RET                                     ; Return
MV15:   LAHF                                    ; Change Pawn to a Queen
        OR      dl,4
        SAHF
        JMP     MV5                             ; Jump
MV20:   MOV     bx,POSQ                         ; Addr of saved Queen position
MV21:   TEST    dl,80h                          ; Is Queen white ?
        JZ      MV22                            ; Yes - jump
        LAHF                                    ; Increment to black Queen pos
        INC     bx
        SAHF
MV22:   MOV     al,byte ptr [ebp+M2]            ; Get new Queen position
        MOV     byte ptr [ebp+ebx],al           ; Save
        JMP     MV5                             ; Jump
MV30:   MOV     bx,POSK                         ; Get saved King position
        TEST    dh,40h                          ; Castling ?
        JZ      MV21                            ; No - jump
        LAHF                                    ; Set King castled flag
        OR      dl,10h
        SAHF
        JMP     MV21                            ; Jump
MV40:   MOV     bx,word ptr [ebp+MLPTRJ]        ; Get move list pointer
        MOV     dx,8                            ; Increment to next move
        LAHF
        ADD     bx,dx
        SAHF
        JMP     MV1                             ; Jump (2nd part of dbl move)

;***********************************************************
; UN-MOVE ROUTINE
;***********************************************************
; FUNCTION:   --  To reverse the process of the move routine,
;                 thereby restoring the board array to its
;                 previous position.
;
; CALLED BY:  --  VALMOV
;                 EVAL
;                 FNDMOV
;                 ASCEND
;
; CALLS:      --  None
;
; ARGUMENTS:  --  None
;***********************************************************
UNMOVE: MOV     bx,word ptr [ebp+MLPTRJ]        ; Load move list pointer
        LAHF                                    ; Increment past link bytes
        INC     bx
        SAHF
        LAHF
        INC     bx
        SAHF
UM1:    MOV     al,byte ptr [ebp+ebx]           ; Get "from" position
        MOV     byte ptr [ebp+M1],al            ; Save
        LAHF                                    ; Increment pointer
        INC     bx
        SAHF
        MOV     al,byte ptr [ebp+ebx]           ; Get "to" position
        MOV     byte ptr [ebp+M2],al            ; Save
        LAHF                                    ; Increment pointer
        INC     bx
        SAHF
        MOV     dh,byte ptr [ebp+ebx]           ; Get captured piece/flags
        MOV     si,word ptr [ebp+M2]            ; Load "to" pos board index
        MOV     dl,byte ptr [ebp+esi+BOARD]     ; Get piece moved
        TEST    dh,20h                          ; Was it a Pawn promotion ?
        JNZ     UM15                            ; Yes - jump
        MOV     al,dl                           ; Get piece moved
        AND     al,7                            ; Clear flag bits
        CMP     al,QUEEN                        ; Was it a Queen ?
        JZ      UM20                            ; Yes - jump
        CMP     al,KING                         ; Was it a King ?
        JZ      UM30                            ; Yes - jump
UM5:    TEST    dh,10h                          ; Is this 1st move for piece ?
        JNZ     UM16                            ; Yes - jump
UM6:    MOV     di,word ptr [ebp+M1]            ; Load "from" pos board index
        MOV     byte ptr [ebp+edi+BOARD],dl     ; Return to previous board pos
        MOV     al,dh                           ; Get captured piece, if any
        AND     al,8FH                          ; Clear flags
        MOV     byte ptr [ebp+esi+BOARD],al     ; Return to board
        TEST    dh,40h                          ; Was it a double move ?
        JNZ     UM40                            ; Yes - jump
        MOV     al,dh                           ; Get captured piece, if any
        AND     al,7                            ; Clear flag bits
        CMP     al,QUEEN                        ; Was it a Queen ?
        JZ      skip22                          ; No - return
        RET
skip22:
        MOV     bx,POSQ                         ; Address of saved Queen pos
        TEST    dh,80h                          ; Is Queen white ?
        JZ      UM10                            ; Yes - jump
        LAHF                                    ; Increment to black Queen pos
        INC     bx
        SAHF
UM10:   MOV     al,byte ptr [ebp+M2]            ; Queen's previous position
        MOV     byte ptr [ebp+ebx],al           ; Save
        RET                                     ; Return
UM15:   LAHF                                    ; Restore Queen to Pawn
        AND     dl,0fbh
        SAHF
        JMP     UM5                             ; Jump
UM16:   LAHF                                    ; Clear piece moved flag
        AND     dl,0f7h
        SAHF
        JMP     UM6                             ; Jump
UM20:   MOV     bx,POSQ                         ; Addr of saved Queen position
UM21:   TEST    dl,80h                          ; Is Queen white ?
        JZ      UM22                            ; Yes - jump
        LAHF                                    ; Increment to black Queen pos
        INC     bx
        SAHF
UM22:   MOV     al,byte ptr [ebp+M1]            ; Get previous position
        MOV     byte ptr [ebp+ebx],al           ; Save
        JMP     UM5                             ; Jump
UM30:   MOV     bx,POSK                         ; Address of saved King pos
        TEST    dh,40h                          ; Was it a castle ?
        JZ      UM21                            ; No - jump
        LAHF                                    ; Clear castled flag
        AND     dl,0efh
        SAHF
        JMP     UM21                            ; Jump
UM40:   MOV     bx,word ptr [ebp+MLPTRJ]        ; Load move list pointer
        MOV     dx,8                            ; Increment to next move
        LAHF
        ADD     bx,dx
        SAHF
        JMP     UM1                             ; Jump (2nd part of dbl move)

;***********************************************************
; SORT ROUTINE
;***********************************************************
; FUNCTION:   --  To sort the move list in order of
;                 increasing move value scores.
;
; CALLED BY:  --  FNDMOV
;
; CALLS:      --  EVAL
;
; ARGUMENTS:  --  None
;***********************************************************
SORTM:  MOV     cx,word ptr [ebp+MLPTRI]        ; Move list begin pointer
        MOV     dx,0                            ; Initialize working pointers
SR5:    MOV     bh,ch
        MOV     bl,cl
        MOV     cl,byte ptr [ebp+ebx]           ; Link to next move
        LAHF
        INC     bx
        SAHF
        MOV     ch,byte ptr [ebp+ebx]
        MOV     byte ptr [ebp+ebx],dh           ; Store to link in list
        LAHF
        DEC     bx
        SAHF
        MOV     byte ptr [ebp+ebx],dl
        XOR     al,al                           ; End of list ?
        CMP     al,ch
        JNZ     skip23                          ; Yes - return
        RET
skip23:
SR10:   MOV     word ptr [ebp+MLPTRJ],cx        ; Save list pointer
        CALL    EVAL                            ; Evaluate move
        MOV     bx,word ptr [ebp+MLPTRI]        ; Begining of move list
        MOV     cx,word ptr [ebp+MLPTRJ]        ; Restore list pointer
SR15:   MOV     dl,byte ptr [ebp+ebx]           ; Next move for compare
        LAHF
        INC     bx
        SAHF
        MOV     dh,byte ptr [ebp+ebx]
        XOR     al,al                           ; At end of list ?
        CMP     al,dh
        JZ      SR25                            ; Yes - jump
        PUSH    edx                             ; Transfer move pointer
        POP     esi
        MOV     al,byte ptr [ebp+VALM]          ; Get new move value
        CMP     al,byte ptr [ebp+esi+MLVAL]     ; Less than list value ?
        JNC     SR30                            ; No - jump
SR25:   MOV     byte ptr [ebp+ebx],ch           ; Link new move into list
        LAHF
        DEC     bx
        SAHF
        MOV     byte ptr [ebp+ebx],cl
        JMP     SR5                             ; Jump
SR30:   XCHG    bx,dx                           ; Swap pointers
        JMP     SR15                            ; Jump

;***********************************************************
; EVALUATION ROUTINE
;***********************************************************
; FUNCTION:   --  To evaluate a given move in the move list.
;                 It first makes the move on the board, then if
;                 the move is legal, it evaluates it, and then
;                 restores the board position.
;
; CALLED BY:  --  SORT
;
; CALLS:      --  MOVE
;                 INCHK
;                 PINFND
;                 POINTS
;                 UNMOVE
;
; ARGUMENTS:  --  None
;***********************************************************
EVAL:   CALL    MOVE                            ; Make move on the board array
        CALL    INCHK                           ; Determine if move is legal
        AND     al,al                           ; Legal move ?
        JZ      EV5                             ; Yes - jump
        XOR     al,al                           ; Score of zero
        MOV     byte ptr [ebp+VALM],al          ; For illegal move
        JMP     EV10                            ; Jump
EV5:    CALL    PINFND                          ; Compile pinned list
        CALL    POINTS                          ; Assign points to move
EV10:   CALL    UNMOVE                          ; Restore board array
        RET                                     ; Return

;***********************************************************
; FIND MOVE ROUTINE
;***********************************************************
; FUNCTION:   --  To determine the computer's best move by
;                 performing a depth first tree search using
;                 the techniques of alpha-beta pruning.
;
; CALLED BY:  --  CPTRMV
;
; CALLS:      --  PINFND
;                 POINTS
;                 GENMOV
;                 SORTM
;                 ASCEND
;                 UNMOVE
;
; ARGUMENTS:  --  None
;***********************************************************
FNDMOV: MOV     al,byte ptr [ebp+MOVENO]        ; Current move number
        CMP     al,1                            ; First move ?
        JNZ     skip24                          ; Yes - execute book opening
        CALL    BOOK
skip24:
        XOR     al,al                           ; Initialize ply number to zero
        MOV     byte ptr [ebp+NPLY],al
        MOV     bx,0                            ; Initialize best move to zero
        MOV     word ptr [ebp+BESTM],bx
        MOV     bx,MLIST                        ; Initialize ply list pointers
        MOV     word ptr [ebp+MLNXT],bx
        MOV     bx,PLYIX-2
        MOV     word ptr [ebp+MLPTRI],bx
        MOV     al,byte ptr [ebp+KOLOR]         ; Initialize color
        MOV     byte ptr [ebp+COLOR],al
        MOV     bx,SCORE                        ; Initialize score index
        MOV     word ptr [ebp+SCRIX],bx
        MOV     al,byte ptr [ebp+PLYMAX]        ; Get max ply number
        ADD     al,2                            ; Add 2
        MOV     ch,al                           ; Save as counter
        XOR     al,al                           ; Zero out score table
back05: MOV     byte ptr [ebp+ebx],al
        LAHF
        INC     bx
        SAHF
        LAHF
        DEC     ch
        JNZ     back05
        SAHF
        MOV     byte ptr [ebp+BC0],al           ; Zero ply 0 board control
        MOV     byte ptr [ebp+MV0],al           ; Zero ply 0 material
        CALL    PINFND                          ; Compile pin list
        CALL    POINTS                          ; Evaluate board at ply 0
        MOV     al,byte ptr [ebp+BRDC]          ; Get board control points
        MOV     byte ptr [ebp+BC0],al           ; Save
        MOV     al,byte ptr [ebp+MTRL]          ; Get material count
        MOV     byte ptr [ebp+MV0],al           ; Save
FM5:    MOV     bx,NPLY                         ; Address of ply counter
        INC     byte ptr [ebp+ebx]              ; Increment ply count
        XOR     al,al                           ; Initialize mate flag
        MOV     byte ptr [ebp+MATEF],al
        CALL    GENMOV                          ; Generate list of moves
        MOV     al,byte ptr [ebp+NPLY]          ; Current ply counter
        MOV     bx,PLYMAX                       ; Address of maximum ply number
        CMP     al,byte ptr [ebp+ebx]           ; At max ply ?
        JNC     skip25                          ; No - call sort
        CALL    SORTM
skip25:
        MOV     bx,word ptr [ebp+MLPTRI]        ; Load ply index pointer
        MOV     word ptr [ebp+MLPTRJ],bx        ; Save as last move pointer
FM15:   MOV     bx,word ptr [ebp+MLPTRJ]        ; Load last move pointer
        MOV     dl,byte ptr [ebp+ebx]           ; Get next move pointer
        LAHF
        INC     bx
        SAHF
        MOV     dh,byte ptr [ebp+ebx]
        MOV     al,dh
        AND     al,al                           ; End of move list ?
        JZ      FM25                            ; Yes - jump
        MOV     word ptr [ebp+MLPTRJ],dx        ; Save current move pointer
        MOV     bx,word ptr [ebp+MLPTRI]        ; Save in ply pointer list
        MOV     byte ptr [ebp+ebx],dl
        LAHF
        INC     bx
        SAHF
        MOV     byte ptr [ebp+ebx],dh
        MOV     al,byte ptr [ebp+NPLY]          ; Current ply counter
        MOV     bx,PLYMAX                       ; Maximum ply number ?
        CMP     al,byte ptr [ebp+ebx]           ; Compare
        JC      FM18                            ; Jump if not max
        CALL    MOVE                            ; Execute move on board array
        CALL    INCHK                           ; Check for legal move
        AND     al,al                           ; Is move legal
        JZ      rel017                          ; Yes - jump
        CALL    UNMOVE                          ; Restore board position
        JMP     FM15                            ; Jump
rel017: MOV     al,byte ptr [ebp+NPLY]          ; Get ply counter
        MOV     bx,PLYMAX                       ; Max ply number
        CMP     al,byte ptr [ebp+ebx]           ; Beyond max ply ?
        JNZ     FM35                            ; Yes - jump
        MOV     al,byte ptr [ebp+COLOR]         ; Get current color
        XOR     al,80H                          ; Get opposite color
        CALL    INCHK1                          ; Determine if King is in check
        AND     al,al                           ; In check ?
        JZ      FM35                            ; No - jump
        JMP     FM19                            ; Jump (One more ply for check)
FM18:   MOV     si,word ptr [ebp+MLPTRJ]        ; Load move pointer
        MOV     al,byte ptr [ebp+esi+MLVAL]     ; Get move score
        AND     al,al                           ; Is it zero (illegal move) ?
        JZ      FM15                            ; Yes - jump
        CALL    MOVE                            ; Execute move on board array
FM19:   MOV     bx,COLOR                        ; Toggle color
        MOV     al,80H
        XOR     al,byte ptr [ebp+ebx]
        MOV     byte ptr [ebp+ebx],al           ; Save new color
        TEST    al,80h                          ; Is it white ?
        JNZ     rel018                          ; No - jump
        MOV     bx,MOVENO                       ; Increment move number
        INC     byte ptr [ebp+ebx]
rel018: MOV     bx,word ptr [ebp+SCRIX]         ; Load score table pointer
        MOV     al,byte ptr [ebp+ebx]           ; Get score two plys above
        LAHF                                    ; Increment to current ply
        INC     bx
        SAHF
        LAHF
        INC     bx
        SAHF
        MOV     byte ptr [ebp+ebx],al           ; Save score as initial value
        LAHF                                    ; Decrement pointer
        DEC     bx
        SAHF
        MOV     word ptr [ebp+SCRIX],bx         ; Save it
        JMP     FM5                             ; Jump
FM25:   MOV     al,byte ptr [ebp+MATEF]         ; Get mate flag
        AND     al,al                           ; Checkmate or stalemate ?
        JNZ     FM30                            ; No - jump
        MOV     al,byte ptr [ebp+CKFLG]         ; Get check flag
        AND     al,al                           ; Was King in check ?
        MOV     al,80H                          ; Pre-set stalemate score
        JZ      FM36                            ; No - jump (stalemate)
        MOV     al,byte ptr [ebp+MOVENO]        ; Get move number
        MOV     byte ptr [ebp+PMATE],al         ; Save
        MOV     al,0FFH                         ; Pre-set checkmate score
        JMP     FM36                            ; Jump
FM30:   MOV     al,byte ptr [ebp+NPLY]          ; Get ply counter
        CMP     al,1                            ; At top of tree ?
        JNZ     skip26                          ; Yes - return
        RET
skip26:
        CALL    ASCEND                          ; Ascend one ply in tree
        MOV     bx,word ptr [ebp+SCRIX]         ; Load score table pointer
        LAHF                                    ; Increment to current ply
        INC     bx
        SAHF
        LAHF
        INC     bx
        SAHF
        MOV     al,byte ptr [ebp+ebx]           ; Get score
        LAHF                                    ; Restore pointer
        DEC     bx
        SAHF
        LAHF
        DEC     bx
        SAHF
        JMP     FM37                            ; Jump
FM35:   CALL    PINFND                          ; Compile pin list
        CALL    POINTS                          ; Evaluate move
        CALL    UNMOVE                          ; Restore board position
        MOV     al,byte ptr [ebp+VALM]          ; Get value of move
FM36:   MOV     bx,MATEF                        ; Set mate flag
        LAHF
        OR      byte ptr [ebp+ebx],1
        SAHF
        MOV     bx,word ptr [ebp+SCRIX]         ; Load score table pointer
FM37:   CALLBACK "Alpha beta cutoff?"
        CMP     al,byte ptr [ebp+ebx]           ; Compare to score 2 ply above
        JC      FM40                            ; Jump if less
        JZ      FM40                            ; Jump if equal
        NEG     al                              ; Negate score
        LAHF                                    ; Incr score table pointer
        INC     bx
        SAHF
        CMP     al,byte ptr [ebp+ebx]           ; Compare to score 1 ply above
        CALLBACK "No. Best move?"
        JC      FM15                            ; Jump if less than
        JZ      FM15                            ; Jump if equal
        MOV     byte ptr [ebp+ebx],al           ; Save as new score 1 ply above
        CALLBACK "Yes! Best move"
        MOV     al,byte ptr [ebp+NPLY]          ; Get current ply counter
        CMP     al,1                            ; At top of tree ?
        JNZ     FM15                            ; No - jump
        MOV     bx,word ptr [ebp+MLPTRJ]        ; Load current move pointer
        MOV     word ptr [ebp+BESTM],bx         ; Save as best move pointer
        MOV     al,byte ptr [ebp+SCORE+1]       ; Get best move score
        CMP     al,0FFH                         ; Was it a checkmate ?
        JNZ     FM15                            ; No - jump
        MOV     bx,PLYMAX                       ; Get maximum ply number
        DEC     byte ptr [ebp+ebx]              ; Subtract 2
        DEC     byte ptr [ebp+ebx]
        MOV     al,byte ptr [ebp+KOLOR]         ; Get computer's color
        TEST    al,80h                          ; Is it white ?
        JNZ     skip27                          ; Yes - return
        RET
skip27:
        MOV     bx,PMATE                        ; Checkmate move number
        DEC     byte ptr [ebp+ebx]              ; Decrement
        RET                                     ; Return
FM40:   CALL    ASCEND                          ; Ascend one ply in tree
        JMP     FM15                            ; Jump

;***********************************************************
; ASCEND TREE ROUTINE
;***********************************************************
; FUNCTION:  --  To adjust all necessary parameters to
;                ascend one ply in the tree.
;
; CALLED BY: --  FNDMOV
;
; CALLS:     --  UNMOVE
;
; ARGUMENTS: --  None
;***********************************************************
ASCEND: MOV     bx,COLOR                        ; Toggle color
        MOV     al,80H
        XOR     al,byte ptr [ebp+ebx]
        MOV     byte ptr [ebp+ebx],al           ; Save new color
        TEST    al,80h                          ; Is it white ?
        JZ      rel019                          ; Yes - jump
        MOV     bx,MOVENO                       ; Decrement move number
        DEC     byte ptr [ebp+ebx]
rel019: MOV     bx,word ptr [ebp+SCRIX]         ; Load score table index
        LAHF                                    ; Decrement
        DEC     bx
        SAHF
        MOV     word ptr [ebp+SCRIX],bx         ; Save
        MOV     bx,NPLY                         ; Decrement ply counter
        DEC     byte ptr [ebp+ebx]
        MOV     bx,word ptr [ebp+MLPTRI]        ; Load ply list pointer
        LAHF                                    ; Load pointer to move list top
        DEC     bx
        SAHF
        MOV     dh,byte ptr [ebp+ebx]
        LAHF
        DEC     bx
        SAHF
        MOV     dl,byte ptr [ebp+ebx]
        MOV     word ptr [ebp+MLNXT],dx         ; Update move list avail ptr
        LAHF                                    ; Get ptr to next move to undo
        DEC     bx
        SAHF
        MOV     dh,byte ptr [ebp+ebx]
        LAHF
        DEC     bx
        SAHF
        MOV     dl,byte ptr [ebp+ebx]
        MOV     word ptr [ebp+MLPTRI],bx        ; Save new ply list pointer
        MOV     word ptr [ebp+MLPTRJ],dx        ; Save next move pointer
        CALL    UNMOVE                          ; Restore board to previous ply
        RET                                     ; Return

;***********************************************************
; ONE MOVE BOOK OPENING
; **********************************************************
; FUNCTION:   --  To provide an opening book of a single
;                 move.
;
; CALLED BY:  --  FNDMOV
;
; CALLS:      --  None
;
; ARGUMENTS:  --  None
;***********************************************************
BOOK:   POP     eax                             ; Abort return to FNDMOV
        SAHF
        MOV     bx,SCORE+1                      ; Zero out score
        MOV     byte ptr [ebp+ebx],0            ; Zero out score table
        MOV     bx,BMOVES-2                     ; Init best move ptr to book
        MOV     word ptr [ebp+BESTM],bx
        MOV     bx,BESTM                        ; Initialize address of pointer
        MOV     al,byte ptr [ebp+KOLOR]         ; Get computer's color
        AND     al,al                           ; Is it white ?
        JNZ     BM5                             ; No - jump
        Z80_LDAR                                ; Load refresh reg (random no)
        CALLBACK "LDAR"
        TEST    al,1                            ; Test random bit
        JNZ     skip28                          ; Return if zero (P-K4)
        RET
skip28:
        INC     byte ptr [ebp+ebx]              ; P-Q4
        INC     byte ptr [ebp+ebx]
        INC     byte ptr [ebp+ebx]
        RET                                     ; Return
BM5:    INC     byte ptr [ebp+ebx]              ; Increment to black moves
        INC     byte ptr [ebp+ebx]
        INC     byte ptr [ebp+ebx]
        INC     byte ptr [ebp+ebx]
        INC     byte ptr [ebp+ebx]
        INC     byte ptr [ebp+ebx]
        MOV     si,word ptr [ebp+MLPTRJ]        ; Pointer to opponents 1st move
        MOV     al,byte ptr [ebp+esi+MLFRP]     ; Get "from" position
        CMP     al,22                           ; Is it a Queen Knight move ?
        JZ      BM9                             ; Yes - Jump
        CMP     al,27                           ; Is it a King Knight move ?
        JZ      BM9                             ; Yes - jump
        CMP     al,34                           ; Is it a Queen Pawn ?
        JZ      BM9                             ; Yes - jump
        JNC     skip29                          ; If Queen side Pawn opening -
        RET
skip29:
                                                ; return (P-K4)
        CMP     al,35                           ; Is it a King Pawn ?
        JNZ     skip30                          ; Yes - return (P-K4)
        RET
skip30:
BM9:    INC     byte ptr [ebp+ebx]              ; (P-Q4)
        INC     byte ptr [ebp+ebx]
        INC     byte ptr [ebp+ebx]
        RET                                     ; Return to CPTRMV


;***********************************************************
; COMPUTER MOVE ROUTINE
;***********************************************************
; FUNCTION:   --  To control the search for the computers move
;                 and the display of that move on the board
;                 and in the move list.
;
; CALLED BY:  --  DRIVER
;
; CALLS:      --  FNDMOV
;                 FCDMAT
;                 MOVE
;                 EXECMV
;                 BITASN
;                 INCHK
;
; MACRO CALLS:    PRTBLK
;                 CARRET
;
; ARGUMENTS:  --  None
;***********************************************************
CPTRMV: CALL    FNDMOV                          ; Select best move
        CALLBACK "After FNDMOV()"
        MOV     bx,word ptr [ebp+BESTM]         ; Move list pointer variable
        MOV     word ptr [ebp+MLPTRJ],bx        ; Pointer to move data
        MOV     al,byte ptr [ebp+SCORE+1]       ; To check for mates
        CMP     al,1                            ; Mate against computer ?
        JNZ     CP0C                            ; No - jump
        MOV     cl,1                            ; Computer mate flag
        CALL    FCDMAT                          ; Full checkmate ?
CP0C:   CALL    MOVE                            ; Produce move on board array
        CALL    EXECMV                          ; Make move on graphics board
                                                ; and return info about it
        MOV     al,ch                           ; Special move flags
        AND     al,al                           ; Special ?
        JNZ     CP10                            ; Yes - jump
        MOV     dh,dl                           ; "To" position of the move
        CALL    BITASN                          ; Convert to Ascii
        MOV     word ptr [ebp+MVEMSG+3],bx      ; Put in move message
        MOV     dh,cl                           ; "From" position of the move
        CALL    BITASN                          ; Convert to Ascii
        MOV     word ptr [ebp+MVEMSG],bx        ; Put in move message
        PRTBLK  MVEMSG,5                        ; Output text of move
        JMP     CP1C                            ; Jump
CP10:   TEST    ch,2                            ; King side castle ?
        JZ      rel020                          ; No - jump
        PRTBLK  O_O,5                           ; Output "O-O"
        JMP     CP1C                            ; Jump
rel020: TEST    ch,4                            ; Queen side castle ?
        JZ      rel021                          ; No - jump
        PRTBLK  O_O_O,5                         ; Output "O-O-O"
        JMP     CP1C                            ; Jump
rel021: PRTBLK  P_PEP,5                         ; Output "PxPep" - En passant
CP1C:   MOV     al,byte ptr [ebp+COLOR]         ; Should computer call check ?
        MOV     ch,al
        XOR     al,80H                          ; Toggle color
        MOV     byte ptr [ebp+COLOR],al
        CALL    INCHK                           ; Check for check
        AND     al,al                           ; Is enemy in check ?
        MOV     al,ch                           ; Restore color
        MOV     byte ptr [ebp+COLOR],al
        JZ      CP24                            ; No - return
        CARRET                                  ; New line
        MOV     al,byte ptr [ebp+SCORE+1]       ; Check for player mated
        CMP     al,0FFH                         ; Forced mate ?
        JZ      skip31                          ; No - Tab to computer column
        CALL    TBCPMV
skip31:
        PRTBLK  CKMSG,5                         ; Output "check"
        MOV     bx,LINECT                       ; Address of screen line count
        INC     byte ptr [ebp+ebx]              ; Increment for message
CP24:   MOV     al,byte ptr [ebp+SCORE+1]       ; Check again for mates
        CMP     al,0FFH                         ; Player mated ?
        JZ      skip32                          ; No - return
        RET
skip32:
        MOV     cl,0                            ; Set player mate flag
        CALL    FCDMAT                          ; Full checkmate ?
        RET                                     ; Return


;***********************************************************
; BOARD INDEX TO ASCII SQUARE NAME
;***********************************************************
; FUNCTION:   --  To translate a hexadecimal index in the
;                 board array into an ascii description
;                 of the square in algebraic chess notation.
;
; CALLED BY:  --  CPTRMV
;
; CALLS:      --  DIVIDE
;
; ARGUMENTS:  --  Board index input in register D and the
;                 Ascii square name is output in register
;                 pair HL.
;***********************************************************
BITASN: SUB     al,al                           ; Get ready for division
        MOV     dl,10
        CALL    DIVIDE                          ; Divide
        DEC     dh                              ; Get rank on 1-8 basis
        ADD     al,60H                          ; Convert file to Ascii (a-h)
        MOV     bl,al                           ; Save
        MOV     al,dh                           ; Rank
        ADD     al,30H                          ; Convert rank to Ascii (1-8)
        MOV     bh,al                           ; Save
        RET                                     ; Return


;***********************************************************
; ASCII SQUARE NAME TO BOARD INDEX
;***********************************************************
; FUNCTION:   --  To convert an algebraic square name in
;                 Ascii to a hexadecimal board index.
;                 This routine also checks the input for
;                 validity.
;
; CALLED BY:  --  PLYRMV
;
; CALLS:      --  MLTPLY
;
; ARGUMENTS:  --  Accepts the square name in register pair HL
;                 and outputs the board index in register A.
;                 Register B = 0 if ok. Register B = Register
;                 A if invalid.
;***********************************************************
ASNTBI: MOV     al,bl                           ; Ascii rank (1 - 8)
        SUB     al,30H                          ; Rank 1 - 8
        CMP     al,1                            ; Check lower bound
        JS      AT04                            ; Jump if invalid
        CMP     al,9                            ; Check upper bound
        JNC     AT04                            ; Jump if invalid
        INC     al                              ; Rank 2 - 9
        MOV     dh,al                           ; Ready for multiplication
        MOV     dl,10
        CALL    MLTPLY                          ; Multiply
        MOV     al,bh                           ; Ascii file letter (a - h)
        SUB     al,40H                          ; File 1 - 8
        CMP     al,1                            ; Check lower bound
        JS      AT04                            ; Jump if invalid
        CMP     al,9                            ; Check upper bound
        JNC     AT04                            ; Jump if invalid
        ADD     al,dh                           ; File+Rank(20-90)=Board index
        MOV     ch,0                            ; Ok flag
        RET                                     ; Return
AT04:   MOV     ch,al                           ; Invalid flag
        RET                                     ; Return

;***********************************************************
; VALIDATE MOVE SUBROUTINE
;***********************************************************
; FUNCTION:   --  To check a players move for validity.
;
; CALLED BY:  --  PLYRMV
;
; CALLS:      --  GENMOV
;                 MOVE
;                 INCHK
;                 UNMOVE
;
; ARGUMENTS:  --  Returns flag in register A, 0 for valid
;                 and 1 for invalid move.
;***********************************************************
VALMOV: MOV     bx,word ptr [ebp+MLPTRJ]        ; Save last move pointer
        PUSH    ebx                             ; Save register
        MOV     al,byte ptr [ebp+KOLOR]         ; Computers color
        XOR     al,80H                          ; Toggle color
        MOV     byte ptr [ebp+COLOR],al         ; Store
        MOV     bx,PLYIX-2                      ; Load move list index
        MOV     word ptr [ebp+MLPTRI],bx
        MOV     bx,MLIST+1024                   ; Next available list pointer
        MOV     word ptr [ebp+MLNXT],bx
        CALL    GENMOV                          ; Generate opponents moves
        MOV     si,MLIST+1024                   ; Index to start of moves
VA5:    MOV     al,byte ptr [ebp+MVEMSG]        ; "From" position
        CMP     al,byte ptr [ebp+esi+MLFRP]     ; Is it in list ?
        JNZ     VA6                             ; No - jump
        MOV     al,byte ptr [ebp+MVEMSG+1]      ; "To" position
        CMP     al,byte ptr [ebp+esi+MLTOP]     ; Is it in list ?
        JZ      VA7                             ; Yes - jump
VA6:    MOV     dl,byte ptr [ebp+esi+MLPTR]     ; Pointer to next list move
        MOV     dh,byte ptr [ebp+esi+MLPTR+1]
        XOR     al,al                           ; At end of list ?
        CMP     al,dh
        JZ      VA10                            ; Yes - jump
        PUSH    edx                             ; Move to X register
        POP     esi
        JMP     VA5                             ; Jump
VA7:    MOV     word ptr [ebp+MLPTRJ],si        ; Save opponents move pointer
        CALL    MOVE                            ; Make move on board array
        CALL    INCHK                           ; Was it a legal move ?
        AND     al,al
        JNZ     VA9                             ; No - jump
VA8:    POP     ebx                             ; Restore saved register
        RET                                     ; Return
VA9:    CALL    UNMOVE                          ; Un-do move on board array
VA10:   MOV     al,1                            ; Set flag for invalid move
        POP     ebx                             ; Restore saved register
        MOV     word ptr [ebp+MLPTRJ],bx        ; Save move pointer
        RET                                     ; Return


;***********************************************************
; UPDATE POSITIONS OF ROYALTY
;***********************************************************
; FUNCTION:   --  To update the positions of the Kings
;                 and Queen after a change of board position
;                 in ANALYS.
;
; CALLED BY:  --  ANALYS
;
; CALLS:      --  None
;
; ARGUMENTS:  --  None
;***********************************************************
ROYALT: MOV     bx,POSK                         ; Start of Royalty array
        MOV     ch,4                            ; Clear all four positions
back06: MOV     byte ptr [ebp+ebx],0
        LAHF
        INC     bx
        SAHF
        LAHF
        DEC     ch
        JNZ     back06
        SAHF
        MOV     al,21                           ; First board position
RY04:   MOV     byte ptr [ebp+M1],al            ; Set up board index
        MOV     bx,POSK                         ; Address of King position
        MOV     si,word ptr [ebp+M1]
        MOV     al,byte ptr [ebp+esi+BOARD]     ; Fetch board contents
        TEST    al,80h                          ; Test color bit
        JZ      rel023                          ; Jump if white
        LAHF                                    ; Offset for black
        INC     bx
        SAHF
rel023: AND     al,7                            ; Delete flags, leave piece
        CMP     al,KING                         ; King ?
        JZ      RY08                            ; Yes - jump
        CMP     al,QUEEN                        ; Queen ?
        JNZ     RY0C                            ; No - jump
        LAHF                                    ; Queen position
        INC     bx
        SAHF
        LAHF                                    ; Plus offset
        INC     bx
        SAHF
RY08:   MOV     al,byte ptr [ebp+M1]            ; Index
        MOV     byte ptr [ebp+ebx],al           ; Save
RY0C:   MOV     al,byte ptr [ebp+M1]            ; Current position
        INC     al                              ; Next position
        CMP     al,99                           ; Done.?
        JNZ     RY04                            ; No - jump
        RET                                     ; Return


;***********************************************************
; POSITIVE INTEGER DIVISION
;   inputs hi=A lo=D, divide by E
;   output D, remainder in A
;***********************************************************
DIVIDE: PUSH    ecx
        MOV     ch,8
DD04:   SHL     dh,1
        RCL     al,1
        SUB     al,dl
        JS      rel027
        INC     dh
        JMP     rel024
rel027: ADD     al,dl
rel024: LAHF
        DEC     ch
        JNZ     DD04
        SAHF
        POP     ecx
        RET

;***********************************************************
; POSITIVE INTEGER MULTIPLICATION
;   inputs D, E
;   output hi=A lo=D
;***********************************************************
MLTPLY: PUSH    ecx
        SUB     al,al
        MOV     ch,8
ML04:   TEST    dh,1
        JZ      rel025
        ADD     al,dl
rel025: SAR     al,1
        RCR     dh,1
        LAHF
        DEC     ch
        JNZ     ML04
        SAHF
        POP     ecx
        RET


;***********************************************************
; EXECUTE MOVE SUBROUTINE
;***********************************************************
; FUNCTION:   --  This routine is the control routine for
;                 MAKEMV. It checks for double moves and
;                 sees that they are properly handled. It
;                 sets flags in the B register for double
;                 moves:
;                       En Passant -- Bit 0
;                       O-O        -- Bit 1
;                       O-O-O      -- Bit 2
;
; CALLED BY:   -- PLYRMV
;                 CPTRMV
;
; CALLS:       -- MAKEMV
;
; ARGUMENTS:   -- Flags set in the B register as described
;                 above.
;***********************************************************
EXECMV: PUSH    esi                             ; Save registers
        LAHF
        PUSH    eax
        MOV     si,word ptr [ebp+MLPTRJ]        ; Index into move list
        MOV     cl,byte ptr [ebp+esi+MLFRP]     ; Move list "from" position
        MOV     dl,byte ptr [ebp+esi+MLTOP]     ; Move list "to" position
        CALL    MAKEMV                          ; Produce move
        MOV     dh,byte ptr [ebp+esi+MLFLG]     ; Move list flags
        MOV     ch,0
        TEST    dh,40h                          ; Double move ?
        JZ      EX14                            ; No - jump
        MOV     dx,6                            ; Move list entry width
        LAHF                                    ; Increment MLPTRJ
        ADD     si,dx
        SAHF
        MOV     cl,byte ptr [ebp+esi+MLFRP]     ; Second "from" position
        MOV     dl,byte ptr [ebp+esi+MLTOP]     ; Second "to" position
        MOV     al,dl                           ; Get "to" position
        CMP     al,cl                           ; Same as "from" position ?
        JNZ     EX04                            ; No - jump
        INC     ch                              ; Set en passant flag
        JMP     EX10                            ; Jump
EX04:   CMP     al,1AH                          ; White O-O ?
        JNZ     EX08                            ; No - jump
        LAHF                                    ; Set O-O flag
        OR      ch,2
        SAHF
        JMP     EX10                            ; Jump
EX08:   CMP     al,60H                          ; Black 0-0 ?
        JNZ     EX0C                            ; No - jump
        LAHF                                    ; Set 0-0 flag
        OR      ch,2
        SAHF
        JMP     EX10                            ; Jump
EX0C:   LAHF                                    ; Set 0-0-0 flag
        OR      ch,4
        SAHF
EX10:   CALL    MAKEMV                          ; Make 2nd move on board
EX14:   POP     eax                             ; Restore registers
        SAHF
        POP     esi
        RET                                     ; Return


_sargon ENDP
_TEXT   ENDS
END

