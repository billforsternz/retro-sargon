sargon1.asm             ;As close to original book as we can make it
sargon2.asm             ;Typos and spelling errors fixed
sargon3.asm             ;Label all PC relative jumps (instead of, for example JR $+5)
  sargon2-to-3.txt      ;Demonstrate that the labels in sargon3.asm are correct
sargon-8080-and-x86.asm ;Add x86 interface (was sargon5.asm)
sargon-x86.asm          ;Automatically generated from sargon-8080-and-x86.asm
sargon-z80.asm          ;Automatically generated from sargon-8080-and-x86.asm, (was sargon4.asm)
sargon-z80.lst          ;Z80 listing after assembling with the excellent zmac.exe cross assembler
sargon-z80-and-x86.asm  ;Automatically generated from sargon-8080-and-x86.asm, (was sargon5b.asm)
sargon-x86-COMPARE.asm  ;Automatically generated from sargon-z80-and-x86.asm, matches sargon-x86.asm

Convert and test script
=======================
Release\convert-8080-to-z80-or-x86.exe -generate_x86 stages\sargon-8080-and-x86.asm stages\sargon-x86.asm stages\sargon-asm-interface.h
Release\convert-8080-to-z80-or-x86.exe -generate_z80 stages\sargon-8080-and-x86.asm stages\sargon-z80-and-x86.asm
Release\convert-8080-to-z80-or-x86.exe -generate_z80_only stages\sargon-z80.asm
Release\convert-z80-to-x86.exe stages\sargon-z80-and-x86.asm stages\sargon-x86-COMPARE.asm stages\sargon-asm-interface-COMPARE.h
fc stages\sargon-x86.asm stages\sargon-x86-COMPARE.asm
fc stages\sargon-x86.asm src\sargon-x86.asm
fc stages\sargon-asm-interface.h stages\sargon-asm-interface-COMPARE.h
fc stages\sargon-asm-interface.h src\sargon-asm-interface.h
fc stages\sargon-z80.asm stages\sargon4.asm
