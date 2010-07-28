all: sort2 extract simpletest A5Cpu.so SSDwriter SSDexploder checktable a5lookup getstate find_kc

sort2: sort2.cpp
	g++ -O2 -o sort2 sort2.cpp -lpthread

extract: extract.cpp
	g++ -o extract extract.cpp

simpletest: simpletest.cpp Bidirectional.cpp
	g++ -o simpletest simpletest.cpp Bidirectional.cpp

getstate: getstate.cpp Bidirectional.cpp
	g++ -o getstate getstate.cpp Bidirectional.cpp

A5Cpu.so:
	cd ../a5_cpu/;./build.sh;cd ../Utilities/;cp ../a5_cpu/A5Cpu.so .

SSDwriter: SSDwriter.cpp
	g++ -O2 -o SSDwriter SSDwriter.cpp

SSDexploder: MultiFileWriter.cpp SSDexploder.cpp MultiFileWriter.h
	g++ -O2 -o SSDexploder MultiFileWriter.cpp SSDexploder.cpp

checktable: A5Cpu.so checktable.cpp
	g++ -O2 -o checktable checktable.cpp ../a5_cpu/A5CpuStubs.cpp -ldl

a5lookup: a5lookup.cpp Bidirectional.cpp SSDlookup.cpp DeltaLookup.cpp
	g++ -g -o a5lookup a5lookup.cpp Bidirectional.cpp SSDlookup.cpp DeltaLookup.cpp \
	../a5_cpu/A5CpuStubs.cpp -ldl

find_kc: find_kc.cpp Bidirectional.cpp TheMatrix.cpp  Bidirectional.h TheMatrix.h
	g++ -o find_kc find_kc.cpp Bidirectional.cpp TheMatrix.cpp

clean:
	rm -f sort2 extract simpletest A5Cpu.so SSDexploder checktable a5lookup find_kc getstate SSDwriter
