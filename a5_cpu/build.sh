#!/bin/sh

g++ -O2 -fPIC -o A5Cpu.so A5Cpu.cpp Advance.cpp -D BUILDING_DLL -lpthread -shared

g++ -O2 -fPIC -o a5cpu_test a5cpu_test.cpp A5CpuStubs.cpp -ldl
