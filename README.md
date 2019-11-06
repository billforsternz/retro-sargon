The book "Sargon, a computer chess program", by Dan and Kathe Spracklen published by Hayden 1978
presents the source code to the classic early chess program Sargon in Z80 assembly language.
This is a project to bring the code back to life in the modern era.

This project isn't finished, but I've made good progress. Currently the input file is
sargon-step6.asm, which is the original, plus some decoration I've invented. This file gets
transformed into an X86 file translated.asm. This program is now calculating a first move
(which happens to be 1.Nf3).

I acknowledge the ownership of legendary
programmers Dan and Kathe Spracklen, and if they contact me in respect of this project, I will
honour their wishes, whatever they may be.

I have previously successfully done something similar with another early program, Microchess
by Peter Jennings. A google search should locate the code for that project, but I should
really bring it over to Github.

Some notes to myself:

To turn on assembly programming in Visual C++ 2017
<br>Menu > Project > build customizations, turn on MASM
<br>Also need linker option LSAFESEH:NO

