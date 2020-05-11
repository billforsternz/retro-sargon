sargon1.asm                     ;As close to original book as we can make it
sargon2.asm                     ;Typos and spelling errors fixed
sargon3.asm                     ;Label all PC relative jumps (instead of, for example JR $+5)
  sargon2-to-3.txt              ;Demonstrate that the labels in sargon3.asm are correct
sargon4.asm                     ;Same as sargon3.asm, but with Z80 mnemonics  
sargon4.lst                     ;Z80 listing after assembling with the excellent zmac.exe cross assembler
sargon-8080-and-x86.asm         ;Add x86 interface to sargon3.asm (was sargon5.asm)
sargon-x86.asm                  ;Automatically generated from sargon-8080-and-x86.asm
sargon-asm-interface.h          ;Companion to sargon-x86.asm
sargon-z80.asm                  ;Automatically generated from sargon-8080-and-x86.asm
sargon-z80.lst                  ;Z80 listing after assembling with the excellent zmac.exe cross assembler
sargon-z80-and-x86.asm          ;Automatically generated from sargon-8080-and-x86.asm

Notes
=====
sargon4.asm was created manually early in the project, when we first got Z80 code generation
going. It is retained as it should match automatically generated sargon-z80.asm (almost exactly).

See the file rebuild-and-compare.bat to generate the generated files.
