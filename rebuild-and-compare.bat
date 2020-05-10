Release\convert-8080-to-z80-or-x86.exe -generate_x86 -relax stages\sargon-8080-and-x86.asm src\sargon-x86.asm src\sargon-asm-interface.h temp-report.txt
copy src\sargon-x86.asm stages
copy src\sargon-asm-interface.h stages
Release\convert-8080-to-z80-or-x86.exe -generate_z80 stages\sargon-8080-and-x86.asm stages\sargon-z80-and-x86.asm temp-interface.h temp-report.txt
Release\convert-8080-to-z80-or-x86.exe -generate_z80_only stages\sargon-8080-and-x86.asm stages\sargon-z80.asm temp-interface.h temp-report.txt
Release\convert-z80-to-x86.exe -relax stages\sargon-z80-and-x86.asm stages\sargon-x86-COMPARE.asm stages\sargon-asm-interface-COMPARE.h temp-report.txt
Release\convert-z80-to-x86.exe -z80_only stages\sargon-z80-and-x86.asm stages\sargon-z80-COMPARE.asm temp-interface.h temp-report.txt
fc stages\sargon-x86.asm stages\sargon-x86-COMPARE.asm
fc stages\sargon-asm-interface.h stages\sargon-asm-interface-COMPARE.h
fc stages\sargon-z80.asm stages\sargon-z80-COMPARE.asm
REM fc stages\sargon-z80.asm stages\sargon4.asm
REM Unfortunately, sargon4.asm and sargon-z80.asm are a bit too different for fc (use winmerge perhaps), other fc commands should reveal no differences
del temp-*.*
