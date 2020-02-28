The book "Sargon, a computer chess program", by Dan and Kathe Spracklen published by Hayden 1978
presents the source code to the classic early chess program Sargon in Z80 assembly language.
This is a project to bring the code back to life in the modern era.

The project is approaching completion. Sargon runs well, it's pretty decent at tactics and
will tear you apart if you don't pay attention. It doesn't understand the endgame - finishing
the opponent off in that stage (even with say K+Q v K) is beyond its horizon. I am tempted
to add a simple "king in a decreasing sized box" type heuristic to the scoring
function to fix that - but that's not really software archaeology is it? A similar problem,
probably fixable in the same way is that Sargon will sometimes drift and concede a draw
by repeating moves even in an overwhelming position.

Some notes on the project organisation. The Sargon assembly language source, in original and
transformed versions is in directory "stages" (the name indicates the stages it has been
through). The following files are present;

- stages/sargon1.asm ;As close to original book as we can make it
- stages/sargon2.asm ;Typos and spelling errors fixed
- stages/sargon3.asm ;Label all PC relative jumps (eg JR $+5 -> JR rel6)
- stages/sargon4.asm ;Conventional Z80 mnemonics instead of TDL assembler mnemonics
- stages/sargon5.asm ;Add x86 interface

There are currently three C++ projects (Visual Studio parlance for C++ executable programs
with main() functions);

1) Transform source code program to convert the assembly language line by line. This program
transforms stages/sargon5.asm into src/translated.asm (it also throws out stages/sargon4.asm
as a nice side effect)
2) A test suite which grew out of a program to get translated.asm working initially, then
working reliably with regression tests.
3) A simple Windows UCI chess engine wrapper so Sargon can run in standard GUIs.

All three projects are working well now. There's a bit more refinement and then I'll publish a
"release" and move on to something else.

I acknowledge the ownership of legendary
programmers Dan and Kathe Spracklen, and if they contact me in respect of this project, I will
honour their wishes, whatever they may be.

I have previously successfully done something similar with another early program, Microchess
by Peter Jennings. A google search should locate the code for that project, but I should
really bring it over to Github.

Some notes to myself:

To turn on assembly programming in Visual C++ 2017
<br>Menu > Project > build customizations, turn on MASM (Only add .asm files after doing this)
<br>Also need linker option SAFESEH:NO (Properties / Linker / All Options / Image has safe exception handlers)
or just use option search facility on "SAFESEH". It's needed because we do long jumps to
exit Sargon if we time out.
