# Host-side simulation of the serial-mouse emulation (stages 4-7).
# Compiles the REAL firmware sources (8250uart.cpp, sermouse.cpp) for the
# host with stub Pico headers, then drives them with guest scenarios.
#   make        build + run
#   make build  build only
CXX      ?= c++
CXXFLAGS ?= -std=c++17 -g -O1 -Wall -Wextra -I stubs -I ..

SRCS = sermouse_sim.cpp ../8250uart.cpp ../sermouse.cpp

run: build
	./sermouse_sim

build: sermouse_sim

sermouse_sim: $(SRCS) ../8250uart.h ../sermouse.h
	$(CXX) $(CXXFLAGS) $(SRCS) -o $@

clean:
	rm -f sermouse_sim
	rm -rf sermouse_sim.dSYM

.PHONY: run build clean
