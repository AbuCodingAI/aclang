adding the AC->LIB and AC LIB headers that dont need a <mainloop> entry point, AC->LIB compiles into a .so or .dll, Imma make the compiler way faster, make the excecution of binaries automatic, make every backend have the same features, do some speed tests, improve the IR, improve the tag system to make tags actually make the language faster, improve cache to separate data and make compiling from cache faster ith a .lir, .irc, and .acc, make .acb files even better, make the experimental binary linker work flawlessly, and finish the machine-audio library that is like pyaudio



here is all the changes we have to do0

machine-audio syntax

library functions have ()
use C++, not python to create the library
say($text$) /*speech synthesis*/
construct(speech) /*text to speech*/
set voice to male /*male voice*/
set voice to female
speech.rate(percentage)
speech.pitch(percentage)
speech.amplitude(percentage)
var=load_mp3(/path/to/mp3)
play(var)
reverb(var, percent)
speed(var, percent)
decode(var)
pause(var)
stop(var)
play.loop(var)
static /*Item, just white noise, you can set a variable to static or even make it the arguement of a function*/
/*AC can't play any noise natually except white noise as of 0.2.1*/
