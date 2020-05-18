Introduction
============

The book "Sargon, a computer chess program", by Dan and Kathe Spracklen
published by Hayden 1978 presents the source code to the classic early
chess program Sargon in Z80 assembly language. This is a project to
bring the code presented in the book back to life in the modern era.

The project comprises four subprojects, two of which do source code
transformation to ultimately produce an x86 assembly language version of
Sargon, and two of which run the resulting Sargon code.

Sargon Executables
==================

Project sargon-engine is a standard UCI chess engine. UCI is a
standardised chess engine interface. Project sargon-engine allows any
modern chess GUI to run Sargon to analyse positions, or play against
other engines or human players. An interesting feature of sargon-engine
is that the original Sargon code was only intended to produce a good
move, but the modern translation peers into the Sargon implementation
and extracts the PV (Principle Variation - the follow up Sargon expects
to its calculated best move), and Sargon's numerical evaluation of the
position. These latter elements weren't required by Sargon's original
user interface, but make for much more interesting user experience when
using a modern chess GUI.

Project sargon-tests is not only a set of unit tests to exercise the
translated Sargon code and make sure it works as intended, but also an
ambitious attempt to open a window into the Sargon implementation to
allow observation of exactly how it works. I anticipate this view into
Sargon's heart could be of interest to programmers starting out on
computer chess even today. After all, Sargon is by necessity much
simpler than modern computer chess implementations, but it uses the
minimax and alpha-beta algorithms that underpin the whole field.

Unless you are interested in the technology story, it's probably best to
skip ahead from here to section "How Well Does it Work?".

Source Code Translation
=======================

Project convert-8080-to-z80-or-x86 is the fundamental source code
translation tool that converts Sargon into something we can run today.
The reference to the older Intel 8080 processor might be confusing. The
Zilog Z80 microprocessor that Sargon 1978 ran on was upwardly compatible
with the older Intel 8080 microprocessor. The Z80 implemented all of the
8080's instruction, plus a whole lot of new ones. In a software
engineering masterstroke, Zilog made life easy for programmers by
designing a much better, more consistent and orthogonal assembly
language than the Intel original for the new, more capable machine. The
Zilog version accommodated all the Intel instructions (of course), and
all the Z80 extensions too in a smooth and consistent way. As an
example, the original 8080 assembly language has different instructions
for all the different types of ADD operations it can do, depending on
where the operands come from. In contrast the Z80 assembly language
replaces them all with just a single ADD mnemonic. The assembler itself
infers which actual machine code ADD instruction (including new ADD
instructions introduced with the Z80) is required from the parameters to
the mnemonic, which are organised in a systematic and consistent way.

That's quite a diversion. The point is though, that Sargon 1978 was not
programmed in Zilog's assembly language. For whatever reason, Sargon's
programmers decided to use the third party TDL assembler which uses
Intel's assembler mnemonics and supports the Z80 extensions with
invented arcane Intel style mnemonics. In the rear vision mirror from
2020 this was a disappointing decision, to say the least. Never mind, it
turned out not to be too much of a road block. After all the Intel style
assembler is harder for the programmer but easier on the source code
translator because there's a unique mnemonic for every instruction. So
it wasn't hard to convert the original code into a more forward looking
version.

In fact as the name implies, convert-8080-to-z80-or-x86 can do two types
of conversion, It can convert to standard Z80 mnemonics, or translate to
Intel x86 assembly language to run on modern machines. The Z80
conversion is not strictly necessary, but I think it's worthwhile to
effectively remove the weird TDL assembler conventions from the equation
for anyone else who want study the original Z80 code.

Similarly, project convert-z80-to-x86 was created to streamline the
workflow of anyone who wants to tinker with the Sargon code in the
future. The idea is that a good approach in the future would be to use
convert-8080-to-z80-or-x86 *once* to convert the original Sargon code to
Z80 mnemonics. Then effectively the original code can be thrown away and
tinkering in Z80 world can continue with the standard Z80 mnemonics
version of the code. Project convert-z80-to-x86 then exists to convert
the modified Z80 code to X86.

For the moment I have not abandoned the original 8080 code in this way.
The Windows script rebuild-and-test.bat converts the original Sargon
code directly to X86 code (8080 -> X86) and via the two step process
(8080 -> Z80 -> X86) and checks that exactly the same sargon-x86.asm
file is created by both routes.

Converting to x86
=================

Intel chose not to provide an 8080 machine code level compatibility mode
for their original 16 bit 8086. (Interesting diversion: One of the
suppliers of 8086 compatible 16 bit microprocessors at the time did
provide a machine level 8080 compatibility mode - the NEC V20). Intel
contented themselves with a very weak level of compatibility, they
simply made one to one translation of assembly language instructions
reasonably straightforward. A good pragmatic decision, but it stands out
as one of the most backward compatibility hostile decisions in the
company's history. Their massively successful X86 family has provided
direct machine level compatibility all the way back to late 1970s 8086
processor ever since.

Amusingly translation was required rather than direct source code
compatibility not only because of some instruction quirks (eg 8086
instructions affecting flags differently to the 8080 equivalents), but
because Intel pulled a Zilog and made the assembly language for the new
chip more consistent and orthogonal. Of course even machine code
compatibility wouldn't help us, as Intel were never going to provide
compatibility to the Z80 extensions.

The bottom line though is that converting either 8080 or Z80 assembly
code to Intel X86 is reasonably straightforward. I chose to go with x86
32 bit mode rather than x64 64 bit mode. Translating to x64 should be
similar and is left as an exercise for the reader :-) Or for me in the
future.

The conversion model used for this project establishes a 64K block of
memory to emulate the Z80's entire memory space within the 32 bit memory
model. Of course 64K is a ludicrously tiny amount of memory today.
Never-the-less it's more than sufficient for Sargon. Sargon's largest
memory requirement by far is a two level move buffer. Two level because
at any given time Sargon is evaluating just one sequence of moves from
the start position, but each move in that sequence is identified in a
separate list of all legal moves from a position in the overall tree of
moves. Z80 Sargon has a max ply depth of 6. It navigates around the full
width move tree (no pruning) without recursion using the simple two
level move buffer. It stores 6 bytes per move node. So its two level
move buffer requires 6 x 6 x MAX bytes of memory where MAX is the
practical maximum number of legal moves in a position. Z80 Sargon
allocates 2K for this, which implies MAX equals about 60. X86 Sargon
extends the max ply depth to 20 and splurges effectively the whole of
the emulated Z80's 64K to the task (equivalent to MAX=500). That renders
the single point where Sargon checks whether it has run out of memory as
completely redundant (just as well as I don't think Sargon would
actually gracefully recover from the memory full condition).

The biggest conversion difficulty is that the Z80 uses 16 bit pointers,
but x86 32 bit mode uses 32 bit pointers My solution to this problem was
to reserve one x86 register ebp to always point at the 64K block of
emulation memory and to perform all memory accesses relative to the ebp
register. This is all very neat and convenient, as the ebp register is
left over after allocating the other x86 pointer registers to emulate
z80 pointer registers (ebx = hl, ecx=bc, edx=de, esi=ix, edi=iy). The
top 16 bits of each of the x86 registers is cleared before entering the
Sargon code and no x86 code is generated that will change the top 16
bits. So we can confidently emulate a (ix+offset) Z80 memory access say
with a (ebp+esi+offset) x86 memory access. This approach should still be
available if an x64 version is made later.

Stack accesses would be more difficult to emulate because although x86
happily accomodates 8 and 16 bit memory accesses in general, all stack
operations in x86 mode are 32 bit. Fortunately Sargon itself doesn't do
any 'tricks' with the stack. It just uses it for straightforward pushes,
pops, calls and returns, and we can emulate that with a normal stack
maintained by our runtime in a completely different part of memory to
our 64K emulation buffer. This speaks to a wider point; the two
conversion programs in this suite have been expressly constructed to
perform the Sargon conversion. They will probably require work to
convert some other 8080 or Z80 program into a working x86 program. Apart
from anything else, instructions that Sargon never uses are not
necessarily converted.

How Well does it Work?
======================

Potential users/tinkerers should have realistic expectations, the
program dates from an era when the humblest club player could beat chess
programs running on consumer grade hardware. I am not an expert on
engine v engine chess, but I did run some matches with my own primitive
engine and I'd estimate sargon-engine.exe's strength at about 1200 Elo.
This was the very first version of Sargon, a project that went through
many more versions, becoming much stronger during the product's
lifecycle. Sargon was an important product in computer chess history,
and I still think the surprising decision the Spracklen's made to make
the source code of their nascent commercial product available is
important and worth celebrating (by bringing that code back to life).

This version of Sargon used fixed depth, full width search. In other
words it did not prune its search tree, considering every single move
out to a fixed depth. The depth was a user selectable option, with 6
(ply) being the maximum setting. On my cheap, aging commodity laptop,
sargon-engine.exe might take something like 20 seconds to perform the 6
ply search in a typical middlegame position. I expect this to be between
two and three orders of magnitude faster than the original Z80 machine,
so I don't imagine many users set the search depth to maximum!

One improvement I have been able to make without changing any of
Sargon's core code is to extend the maximum search depth from 6 to 20
(because I am not limited to a few K of memory) and make the search
depth dynamic rather than fixed. I use an iterative approach. When the
GUI asks the engine to analyse a position I start with depth=3 (which is
essentially instantaneous), present the results, then move to depth=4,
and so on. If the GUI wants the engine to analyse indefinitely
(kibitzing) I just continue iterating all the way up to depth=20. It's
important to understand though that the time required increases
exponentially. Seconds for depth 4, minutes by depth 7, hours by depth
9, days (10), weeks (11), years probably (13 or so). Basically it will
never get to 20. Theoretically the minimax algorithm (which Sargon
implements effectively - not quite perfectly because it doesn't consider
under-promotion) can 'solve chess perfectly'. But in fact chess will
never be absolutely solved unless we can somehow turn every atom in the
universe into a computer co-operating on solving the problem (and not
even then). This is why we can't have nice things.

During human v engine or engine v engine operation the GUI tells sargon-
engine the time left for the game and I use a simple adaptive algorithm
to decide when to stop iterating and present the best move discovered to
date.

The UCI interface allows the user to use the GUI to set engine
parameters, and I have implemented a FixedDepth parameter which allows
emulation of the original Sargon program's behaviour. If FixedDepth is
set to a non-zero number then sargon-engine doesn't iterate at all when
playing a game, it just goes straight to the assigned depth. This
actually has some efficiency advantages, since iterating from depth 5
(say) to depth 6 was not contemplated in the original Sargon code at
all, so when I do that I am in effect discarding all of the depth 5
information and starting again. That sounds terrible, but remember the
exponential growth means that the depth 6 required time will far exceed
the sum of depth 3,4 and 5 required time, so relatively speaking it's
not wasting that much time. Iteration works well and is very pragmatic
and effective in this Sargon implementation.

It might sound that extending Sargon's search depth well beyond 6 hasn't
been very useful because the exponential growth makes levels beyond 8 or
so inaccessible in practice. This would be true if chess stopped in the
middlegame, but as more and more pieces come off the board, there are
less moves to look at in each position, and Sargon can see deeper in a
reasonable time. This is the real beauty of adaptive rather than fixed
depth. As a simple example consider the position with White (to play)
king on e4, pawn on d4, Black king on d7. A serious human player will
instantaneously play Kd5 here, knowing that this is the only way to
eventually force pawn promotion. Sargon, with absolutely no chess
knowledge, just the ability to calculate variations in search of extra
material or board control, plays Kd5 at depth 13 (or more). The PV
associated with this move features the pawn queening triumphantly right
at the end of the line. It takes 3 minutes and 30 seconds to make the
calculation on my computer. It's hardly world shattering but it's vastly
beyond the capacity of the original Z80 implementation, and shows what a
faster CPU and more memory can do to an otherwise fixed chess
calculation algorithm.

However there's no doubt that in general the horizon effect, as it is
called, is a massive limitation on the strength of an engine that does a
full width, fixed depth search with no pruning. Sargon runs well enough.
It plays ugly anti-positional chess, but it does try to mobilise it's
pieces and it's pretty decent at tactics and will sometimes tear you
apart in a complex middlegame if you don't pay attention. But then it
might not be able to finish you off. Despite being able to search deeper
it still doesn't understand endgames because promoting pawns takes so
many ply. And even winning with say K+Q v K is beyond its horizon unless
the opponent's king is already corralled with Sargon's king nearby. I am
tempted to add a simple "king in a decreasing sized box" type heuristic
to the scoring function to fix that - but that's not really software
archaeology is it? A similar problem, probably fixable in the same way
is that Sargon will sometimes drift and concede a draw by repeating
moves even in an overwhelming position. This is a reflection of Sargon's
scoring function, which doesn't have any positional knowledge. Sargon
just tries to win material, and if that's not possible control more
squares than the opponent. This was typical of the era, in the early
days of chess programming the pioneers were delighted to find that
minimax plus alpha beta and a simple material plus board control scoring
function was sufficient to play at least reasonable chess. But it was
only a starting point for real progress.

How do I run it?
================

I have used Windows as my development platform and target, so ideally
you use or have access to a Windows computer. I imagine it would be
relatively straightforward to use Linux, although I am not sure about
the 32 bit development story on Linux with modern distributions. MacOS
is definitely problematice since Apple recently decided to no longer
support 32 bit code at all.

To run sargon-engine.exe effectively it is best to use a Chess GUI. The
Chess GUI serves to provide a user interface to sargon-engine.exe or any
other chess engine equipped with a standard chess engine interface. The
original Z80 Sargon came with its own simple user interface, but I have
elected not to bring that part of the code over, a modern chess GUI
provides a much better experience and the real interest in the Sargon
code is the inner chess calculation code.

I have used two Chess GUIs with sargon-engine.exe, my own Tarrasch Chess
GUI and Arena. I used Arena to run sargon-engine.exe against other chess
engines (Tarrasch unfortunately does not support engine versus engine
operation).

The other Sargon executable in this project is sargon-tests.exe. It
includes a collection of regression tests which initially helped me get
the Sargon port working, and then kept the development firmly on track.
More interesting perhaps is some 'executable documentation' built into
the sargon-tests.exe program. This is an attempt to open a window into
Sargon's implementation of minimax and alpha-beta, the fundamental chess
algorithms. The idea is that a developer starting out in chess
programming might find it very interesting to see a simple working model
of these algorithms in action. The executable documentation can be
executed by running sargon-tests.exe with a -doc command line flag. The
resulting output is available in the repository as sargon-tests-doc-
output.txt

Development Environment
=======================

I have used Visual Studio 2017, although any system that can build C++
programs with a single X86 assembly module should be fine. I have
included a VS2017 solution and four project files in the repository, but
there's no magic involved anywhere. By far the most important
information in the solution and project files is that the individual
components are constructed as follows;

- sargon-engine = sargon-engine.cpp + sargon-x86.asm + sargon-interface.cpp + sargon-pv.cpp + thc.cpp + util.cpp
- sargon-tests = sargon-tests.cpp + sargon-x86.asm + sargon-interface.cpp + sargon-minimax.cpp + sargon-pv.cpp + thc.cpp + util.cpp
- convert-8080-to-z80-or-x86 = convert-8080-to-z80-or-x86.cpp + convert-8080-to-z80-or-x86-main.cpp + util.cpp
- convert-z80-to-x86 = convert-z80-to-x86.cpp + util.cpp

I should mention a couple of small roadblocks I overcame in creating the
project files;

To turn on assembly programming in Visual C++ 2017
<br>Menu > Project > build customizations, turn on MASM (Only add asm files after doing this)

We also need linker option SAFESEH:NO (Properties / Linker / All Options
/ Image has safe exception handlers) or just use option search facility
on "SAFESEH". It's needed because we do long jumps to cut short Sargon's
move calculation if it takes too long, or if the GUI calls on it to
stop.

Further Reading
===============

- [Background information](https://www.chessprogramming.org/Sargon)
There's lots of background information on Sargon here. I hope this page
will soon have a link to this project

- [Scan of original book](http://web.archive.org/web/20070614114334/http://madscientistroom.org/chm/Sargon.html)
I have a paper copy of the book I picked up from abebooks.com, but I am
happy to have access to this electronic copy as well, particularly since
my paper copy of the book has had the TDL assembler reference material
at the end ripped out!

- [Stack Overflow Discussion with Andre Adrian](https://stackoverflow.com/questions/2038929/where-can-i-find-a-8080-to-x86-assembler-conversion-tool)
I first contemplated this project around 2010, and asked for advice on
Z80 conversion tools in this Stackoverflow post. The page includes quite
a lot of background information about the project. I mention Andre
Adrian, a German programmer who gave me the idea for the project and who
contributes to the discussion. Inevitably and sadly, the discussion was
closed as not meeting Stackoverflow guidelines. It makes me happy to
contemplate that I have finally written my own conversion tools and got
this project done after putting it on the back burner for so many years.

- [Andre Adrian's Z80 port](http://www.andreadrian.de/schach/sargon.asm)
I used this, plus the scan of the book above and my own paper copy to
get to stages/sargon1.asm which is the starting point of my conversion
pipeline, being as close to the exact text of the Sargon program in the
book as I can make it.

Andre Adrian's work first came to my attention when I noticed he had
added a chess engine interface to my port of Microchess by Peter
Jennings, an even older 1970s chess program which I ported to Windows
around 2006. It is interesting that in that earlier project I ported the
original user interface of the program and Andre Adrian improved on my
work by adding a chess engine interface. Andre Adrian ported Sargon
including its original user interface to CP/M so that it could be used
with a CP/M emulator. Now I have in turn improved his project (I think)
in the same way he improved mine by replacing the original user
interface with a modern chess engine interface.

A google search should find my earlier Microchess project. I should
really bring it over to Github.

Conversion Pipeline
===================

The directory stages/ contains the conversion process, starting with the
unmodified program text from the book, progressing through manual and
mechanical modification through to the ultimate sargon-x86.asm file (and
its companion sargon-asm-interface.h). The directory contains its own
readme.txt file to describe them. There's also rebuild-and-test.bat
script in the project root directory to repeat and test the conversion
process.

Acknowledgment of Original Authorship
=====================================

I acknowledge the ownership of legendary programmers Dan and Kathe
Spracklen. When I started on the project I added that if they contact me
in respect of this project, I will honour their wishes, whatever they
may be. Since then though I have put a huge amount of effort into
polishing this into a model project. I've opened up a window into the
program, and brought it back to life. I think I would have a great deal
of difficulty deleting this project now, if I was asked to do that.
Hopefully a purely hypothetical question. There is clearly no commercial
motive of incentive for anyone in this project. As I wrote on Twitter "I
am porting a 70s chess program written in Z80 assembly to run on modern
machines with a standard chess engine interface. Never has a project
been more guaranteed not to generate a single cent of revenue."

