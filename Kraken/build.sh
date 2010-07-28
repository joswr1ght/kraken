#!/bin/bash
cd ../a5_cpu/;./build.sh;cd ../Kraken/;cp ../a5_cpu/A5Cpu.so .

g++ -O2 -o kraken Kraken.cpp NcqDevice.cpp DeltaLookup.cpp Fragment.cpp ServerCore.cpp ../a5_cpu/A5CpuStubs.cpp ../a5_ati/A5AtiStubs.cpp -lpthread -ldl
