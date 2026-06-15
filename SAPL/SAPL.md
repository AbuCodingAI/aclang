APL is impossible to type but actually really elegant
like in a typeable form (+/w)/pw for mean, but why doesn't it just use ∑ω​, like you have 30 impossible characters surely sigma couldnt hurt

but for ∑ω​, you can type Ew, so Ew/pw or alternately Ew%pw but / is cleaner and more mathamatically correct

so that bore the idea Standard APL, SAPL

That’s actually a pretty coherent language-design observation, not just “APL bad because symbols.”

Classic APL optimized extremely hard for:

density,
algebraic composition,
visual terseness,
and minimizing parsing ambiguity.

But it paid for that with:

impossible keyboard ergonomics,
unreadability for outsiders,
and symbol memorization overhead.

Your criticism is basically:

“If you already require a custom glyph ecosystem, why didn’t APL choose symbols that map more naturally to mathematical intuition and typing fallback?”

That’s a legitimate critique.

For weighted mean:

∑p
ω
	​

∑ω
	​


your:

Ew/pw
or
Ew%pw

is immediately more discoverable to most programmers than:

(+/w)/pw

because:

E visually evokes expectation / summation,
/ already feels algebraic,
and the structure resembles textbook notation.

APL’s +/ comes from its reduction operator model:

/ = reduce/fold
+ = operation being reduced
so +/w literally means “reduce w with addition.”

Conceptually elegant.
Humanly hostile.

You’re basically proposing:

preserve the algebraic compactness,
remove the alien glyph dependency,
preserve ASCII-first typing,
and allow Unicode as optional enhancement instead of mandatory syntax.

That is very close to what several “post-APL” languages tried to do:

J → ASCII-only APL descendant
K / q → terse finance-oriented descendants
BQN → modern APL cleanup
Julia → Unicode-friendly but typeable scientific syntax
MATLAB → mathematical readability over symbolic purity

But your “SAPL” idea is interesting because it’s not merely:

“APL but ASCII.”

It’s more:

“APL re-centered around mathematically mnemonic notation.”

That’s a different philosophy.

For example:

Concept	APL	SAPL-like
Sum reduce	+/x	Ex
Product reduce	×/x	Px maybe
Mean	(+/x)÷⍴x	Ex/nx
Weighted mean	(+/w)÷pw	Ew/pw
Inner product	+.×	maybe dot or Σ×

APL optimizes for:

parser purity,
operator orthogonality,
symbolic consistency.

You’re optimizing for:

cognitive compression,
mathematical intuition,
keyboard survivability,
and recognizability.

That’s honestly closer to how humans naturally parse notation.

Pasted text(9).txt
Document

Because “high vs low level” is not about how many functions exist.

It’s about:

how much semantic meaning each operation carries,
and how far the language is from hardware reality.

For example:

E I 1 x

looks tiny.

But semantically it means:

sum(
    interval(
        1,
        x
    )
)

That is an enormous amount of information compared to assembly.

Assembly equivalent:

MOV acc, 0
MOV i, 1
loop:
ADD acc, i
INC i
CMP i, x
JLE loop

Notice what happened:

reduction semantics disappeared,
interval semantics disappeared,
associativity disappeared,
mathematical meaning disappeared.

Now the compiler just sees control flow.

SAPL still sees:

reduction(sum, interval(1,x))

That is VERY high-level semantically.

The confusion comes from this:

You’re thinking:

“few primitives = low level”

But actually:

combinatory systems,
array algebras,
lambda calculus,
APL,
tensor IRs,

often have few primitives but extremely high semantic density.

For example:

Lambda Calculus has almost nothing,
but is incredibly high-level mathematically.

Similarly:

x +.* y

is not low-level at all.

It encodes:

pairwise multiplication,
reduction,
dimensional compatibility,
algebraic associativity,
vector semantics,
possibly matrix multiplication.

A low-level IR would instead contain:

pointer arithmetic,
memory loads,
loop counters,
branches,
accumulators.

So SAPL is:

syntactically minimal,
semantically dense.

That combination feels “low-level” emotionally,
but compiler-theoretically it’s actually fairly high-level.

The real low-level version of SAPL would look more like:

alloc
load
store
jump
phi
add
mul
cmp

with no concept of:

reductions,
intervals,
shapes,
transposes,
or dot products.

Once a language understands:

dot product
transpose
reduction
shape

it’s already pretty high in abstraction level.

well does low level nessacarily mean must interact with registers, must offload things, or can it mean, multiple abstractions per higher level functions/lines

Yes — that second definition is actually much closer to how PL theory often treats abstraction level.

“Low-level” does not strictly mean:

registers,
manual memory,
assembly,
or hardware interaction.

Those are just one kind of low-levelness:

hardware proximity.

But another kind exists:

abstraction granularity.

That’s the one you’re intuitively pointing at.

For example:

mean(x) = E x / pw x

contains multiple abstractions:

reduction,
shape query,
division,
implicit traversal.

Whereas:

E x

is much more primitive.

And:

x +.* y

is somewhere in between:

it’s one operation syntactically,
but semantically expands into multiple behaviors.

So SAPL has:

few primitives
but each primitive still carries rich semantics.

That places it in an unusual zone:

low surface abstraction count,
high semantic abstraction density.

This is why languages like APL or Lambda Calculus feel “primitive” and “high-level” simultaneously.

Very few constructs.
Very powerful constructs.

A useful way to think about “level” is:

Type	Meaning
Hardware level	proximity to CPU/registers
Semantic level	how much meaning operations encode
Abstraction granularity	how much one line expands into
Runtime level	how much runtime machinery exists

SAPL:

far from hardware,
high semantic density,
low primitive count,
low runtime complexity.

That combination is rare.

In fact, your compiler vision is almost:

“Extremely small semantic kernel.”

Meaning:

tiny operator vocabulary,
massive compositional power.

That’s architecturally closer to:

combinator calculi,
tensor algebra systems,
array calculi,
symbolic IRs,
than to normal programming languages.

So depending on which definition you use:

Perspective	SAPL
Hardware proximity	high-level
Primitive count	low-level-ish
Semantic density	high-level
Surface syntax complexity	low-level
Optimization layer	mid-level IR

Which is why it feels paradoxical.

mean(x) = sum of x divided by pw x what does it all mean

It means:

mean(x) = E x / pw x

expands into:

mean(x)
=
(sum of all elements in x)
/
(number of elements in x)

because:

SAPL	Meaning
E x	summation reduction of x
pw x	shape/length of x

So if:

x = [1,2,3,4]

then:

E x

means:

1 + 2 + 3 + 4
= 10

and:

pw x

means:

4

because there are 4 elements.

So:

mean(x) = E x / pw x

becomes:

10 / 4
= 2.5

The important thing is:
you are defining the operation mathematically instead of procedurally.

Not:

sum = 0;
for(...)

But instead:

mean =
(sum of array)
divided by
size of array

That’s why SAPL feels algebraic instead of imperative.


plz summarize this