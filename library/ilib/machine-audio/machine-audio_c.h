/* AC ilib: machine-audio — compatibility forward.
   The compiler's C/C++ include codegen appends "_c.h" to the ilib's DIRECTORY name verbatim
   (`ln + "_c.h"`, ir_codegen.cpp's emitHeader), which for this ilib produces the HYPHENATED
   "machine-audio_c.h" — but the real header lives at "machine_audio_c.h" (underscore), matching
   this ilib's C++ core file naming (machine_audio.cpp/.hpp), not its ilib directory name. Every
   OTHER hyphenated ilib either already matches this convention (native-cpu_c.h, web-server_c.h)
   or ships both spellings (string-cheese); this file gives machine-audio the same compatibility
   shim instead of special-casing the compiler for one ilib's internal naming choice.
*/
#pragma once
#include "machine_audio_c.h"
