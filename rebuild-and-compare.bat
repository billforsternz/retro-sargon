REM Do 8080 -> Z80 -> X86 and 8080 -> X86 conversions
Release\convert-8080-to-z80-or-x86.exe -generate_x86 -relax stages\sargon-8080-and-x86.asm stages\sargon-x86.asm stages\sargon-asm-interface.h temp-report.txt
Release\convert-8080-to-z80-or-x86.exe -generate_z80 stages\sargon-8080-and-x86.asm stages\sargon-z80-and-x86.asm temp-interface.h temp-report.txt
Release\convert-8080-to-z80-or-x86.exe -generate_z80_only stages\sargon-8080-and-x86.asm stages\sargon-z80.asm temp-interface.h temp-report.txt
Release\convert-z80-to-x86.exe -relax stages\sargon-z80-and-x86.asm temp-sargon-x86.asm temp-sargon-asm-interface.h temp-report.txt
Release\convert-z80-to-x86.exe -z80_only stages\sargon-z80-and-x86.asm temp-sargon-z80.asm temp-interface.h temp-report.txt

REM Assemble the Z80 code with ZMAC cross assembler to stages\sargon-z80.lst
zmac.exe --oo lst -c --od stages stages\sargon-z80.asm

REM Check whether anything has changed, also use git status
fc stages\sargon-x86.asm temp-sargon-x86.asm
fc stages\sargon-asm-interface.h temp-sargon-asm-interface.h
fc stages\sargon-x86.asm src\sargon-x86.asm
fc stages\sargon-asm-interface.h src\sargon-asm-interface.h
fc stages\sargon-z80.asm temp-sargon-z80.asm
del temp-*.*
