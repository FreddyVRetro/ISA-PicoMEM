:: Compile the Arduino MIDI Sound Module firmware to JavaScript (via Emscripten) for private testing and tools.

setlocal

set SrcPath=%CD%
emcc --bind -o %CD%\firmware.js -O0 -g -std=c++14 -DF_CPU=16000000 -I%SrcPath%\emscripten %SrcPath%\emscripten\avr\mocks.cpp %SrcPath%\emscripten\bindings.cpp

endlocal
