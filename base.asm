	.686P
	.XMM
	include listing.inc
	.model	flat

INCLUDELIB OLDNAMES

_DATA   SEGMENT  ;ALIGN(256)
xx      DB  3,4,5
yy      DW  100h
vv      EQU $
        DD  0ffffffffh
_DATA   ENDS

_BSS    SEGMENT
    DB 100H DUP (?)
    db 0
_BSS    ENDS

PUBLIC	_base_function				; simple_function
_TEXT	SEGMENT
_base_function PROC				; simple_function, COMDAT
    push    esi
ww: mov     ax,yy
zz: mov     ax,[yy]
    lea     esi, offset byte ptr xx
    mov     esi, offset xx
    mov     ax,0
    mov     al,[esi]
    mov     al,6
    mov     [esi],al
	mov	    eax, 34					; 00000022H
    pop     esi
	ret	0
_base_function ENDP				; simple_function

EXTERN _inspector: PROC

PUBLIC	_inspect
_inspect PROC
    push    ebp
    mov     ebp,esp
    push    eax
    push    ebx
    push    ecx
    push    edx
    push    esi
    push    edi
    mov     ebx,[ebp+4]
    mov     esi,ebx
ins01:
    mov     al,[ebx]
    inc     ebx
    cmp     al,0
    jnz     ins01
    mov     [ebp+4],ebx
    push    esi
    call    _inspector
    add     esp,4
    pop     edi
    pop     esi
    pop     edx
    pop     ecx
    pop     ebx
    pop     eax
    pop     ebp
	ret
_inspect ENDP

PUBLIC	_test_inspect
_test_inspect PROC
    push    ebp
    push    eax
    push    ebx
    push    ecx
    push    edx
    push    esi
    push    edi
    call    _inspect
    db      "testing", 0
    pop     edi
    pop     esi
    pop     edx
    pop     ecx
    pop     ebx
    pop     eax
    pop     ebp
	ret
_test_inspect ENDP

_TEXT	ENDS
END
