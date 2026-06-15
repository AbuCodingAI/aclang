## AC

Binary Library full AC parity with dynamic linking or full copying of the library

All backend parity except Backends with special features such as JS/HTML Browser functions

All Libraries working properly

All Errors showing instead of parsing incorrectly, becoming garbage, or segfaulting

AC/AI syntax parity(Except AC->__ and AI->VM)

AC syntax working in AC+

Bootstrapping(Eventually)

<shutoff> working(done when /stop is parsed)

All math(Under 15 Million operations, at this limit, instead of crashing, it will parse the operation at runtime) folding at compile time properly if values are known(For correct flags, some flags make it fold at runtime instead)

Translation from AC features to AC* higher level features

Documents like CHANGELOG and README perfect

Version AST cache like IR cache

Make .irc and .lir where the program exited lowering

Make every file as efficient as cpp-ly possible for C++ 17

LineUp statements(match statements basically)

const value=2

LineUp value {
    1:Term.display $One$
    2:Term.display $Two$
    default: Term.display $Other$
}

AC has cond but also support LineUp syntax as well

export statements

so for AC files that aren't lib files, you can export a function so other files can use it

```math.ac
    AC->BNY
    Make add func(a, b)
        a+b
    export add
```

```main.ac
    AC->C++
    use flib math.ac
    add(5, 2)
```

for AC LIB and AC->LIB, all functions are "exported" automatically

lambdas

```add.ac
    add=Make func(a, b) {a+b}
```

closures

```return.ac
    return Make func() {
        count = count + 1
        return count
    }
```

spawn & wait

```ac
    Make worker func(id)
        Term.display $Worker$ + id
    spawn worker(1)
    spawn worker(2)
    spawn worker(3)

    wait
```

channel

```ac
    spawn channel as ch, name sequence(1, 10)
    rename ch 5, message
    send("Hello", message)
    message=recieve message
    Term.display message
```
notes: ch can be anything, you dont even need an as clause, name can be like, name message1, message2, message3, message4 and it makes that many channels, for sequence(1, 10) it makes 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 named channels
LSP for AC

each

```ac
list=[1, 3, 5, 7, 9, 2, 4, 6, 8, 10]
FOR each item in list, 1
    Term.display item
```
this returns 3, 7, 2, 6, 10(skips one item in the list)

iterating over arrays and matrices

math.i operations solid

```ac
AC->BNY
use ilib math
using header math
<mainloop>
    x=pow(e, i*pi)
    Term.display x
<mainloop>
```
should return 1

recursive process

```fb.ac
AC->BNY
<mainloop>
    fb=Make func() {fb and fb and fb}
<mainloop>
/*This is a dangerous program, AC will mark this and report Preposterous: ForkBomb detected, function only calls itself and does nothing else*/
```
add ^ for exponentiation
5^3 should return 125, this should be in core AC
math.pow is faster though, math.pow uses faster methods than ^, but ^ compiles to all backends properly, like ** for Python



now AC IS a high level language yes, BUT has ONE unique feature

Pointers, yeah, pointers
```ac
x=92

y=ptr x

z=75
```
a=ptr(x, z)
y stores the pointer where x is
a stores 2 pointers in a map value

improve the web ilib from my custom python program to usable ilib, also note, do not change my syntax, syntax is noted in the help function of the web ilib, make the help function for ALL ilibs, so camera, gl, machine-audio, math, os, regex, string-cheese, web, and widgets

from ilib widgets import Screen type syntax for ALL backends

AC* has no pointers

length function(length in AC, len is most languages)

Create Bi using AC(Easy to learn turing complete language, interpreted, Successor of A syntax, is interpreted, Bi is Latin for 2)

```bi
say($Hello World$)
library random
numbers=[1,2,3,4,5,6,7,8,9,0]
for i in range(length(list));
    say(random.choice(list))
if getline is 5{
    say($Goodbye$)
    halt
}
```

use ilib math as x /*Changes the header to x*/
x.pi

using math.pi /*Only allows pi, everything else needs the math header*/

from ilib math use math.pi /*Uses math.pi only*/

full *`.datac`* support(Some changes were made to the format)

## AI

Bytecode->Excecution works on multiple OSes

Full AC->AI syntax parity

Full library parity

## AC+

AOS boots

AC syntax support

All AC+ Bash and ASM like feautures work

.acx can run

## AC*

Full AC syntax->AC* syntax noted

AC libraries work the same in AC*

Full SAPL lowering

Full IR handling

Compiles to every AC backend except ASM

AC compiler, parser etc.(Eventually)

change file extension from .aca(AC Asterisk) to .acs(AC Star)

## JaSQL/AbuDB

.xlsx, .json, .yaml, .datac full support

## SAPL

More syntax to fully support AC* lowering

## IoT

Registers, Compiler, Documents, Examples, Syntax etc. etc.





Make a full AC improvement doc

