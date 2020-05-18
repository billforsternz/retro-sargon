sargon1.asm                     ;As close to original book as we can make it
sargon2.asm                     ;Typos and spelling errors fixed
sargon3.asm                     ;Label all PC relative jumps (instead of, for example JR $+5)
  sargon2-to-3.txt              ;Demonstrate that the labels in sargon3.asm are correct
sargon-8080-and-x86.asm         ;Add x86 interface to sargon3.asm (was sargon5.asm)
sargon-x86.asm                  ;Automatically generated from sargon-8080-and-x86.asm
sargon-asm-interface.h          ;Companion to sargon-x86.asm
sargon-z80.asm                  ;Automatically generated from sargon-8080-and-x86.asm
sargon-z80.lst                  ;Z80 listing after assembling with the excellent zmac.exe cross assembler
sargon-z80-and-x86.asm          ;Automatically generated from sargon-8080-and-x86.asm

Notes
=====
See the file rebuild-and-compare.bat in the project root directory to
generate the generated files. The most efficient way to hack on the
project in the future is to not use the TDL/8080 version of the code and
the associated convert-8080-to-z80-and-x86 tool.

Instead use sargon-z80-and-x86.asm if you want to work both in Z80 and
X86 worlds, use sargon-z80.asm if you only want to work in the Z80 world
and use sargon-x86 if you only want to work in the X86 world.
