Initially
=========
sargon1.asm   ;As close to original book as we can make it
sargon2.asm   ;Typos and spelling errors fixed
sargon3.asm   ;Label all PC relative jumps (instead of, for example JR $+5)
  sargon2-to-3.txt  ;Demonstrate that the labels in sargon3.asm are correct
sargon4.asm   ;Z80 mnemonics
sargon4.lst   ;Z80 listing after assembling with the excellent zmac.exe cross assembler
sargon5.asm   ;Add x86 interface
sargon5b.asm  ;Version of sargon5.asm with Z80 mnemonics (created manually by applying
              ; sargon3.asm->sargon5.asm deltas to sargon4.asm)
../translated.asm   ;X86 version generated from sargon5.asm

Evolving
========
sargon5.asm         ;See above, starting point for this phase
sargon-x86.asm      ;Automatically generated from sargon5.asm, previously ../translated.asm
                    ;cmd>Release\project-convert-sargon-to-x86.exe stages\sargon5.asm stages\sargon-x86.asm
sargon-z80.asm      ;Automatically generated from sargon5.asm, previously sargon4.asm
                    ;cmd>Release\project-convert-sargon-to-x86.exe -generate_z80 stages\sargon5.asm stages\sargon-z80.asm
                    ;(currently Release\project-convert-sargon-to-x86.exe -generate_z80 stages\sargon3.asm sargon-z80.asm)
sargon6.asm         ;Automatically generated from sargon5.asm, previously sargon5b.asm
sargon-z80-x86.asm  ;Automatically generated from sargon5.asm, matches sargon-x86.asm exactly  

