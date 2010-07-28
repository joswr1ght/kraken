
DEPS = BaseReader.h DeltaReader.h Factory.h MultiFileWriter.h \
	BaseWriter.h  DeltaWriter.h  MultiFileReader.h  SSDreader.h \
	SSDwriter.h

SRC = DeltaReader.cpp SSDreader.cpp SSDwriter.cpp \
	DeltaWriter.cpp  MultiFileReader.cpp \
	Factory.cpp      MultiFileWriter.cpp \
	Md5Writer.cpp md5.cpp IndexWriter.cpp

all: TableCompare TableConvert TableDump

TableCompare: $(DEPS) $(SRC) TableCompare.cpp
	g++ -O2 -o TableCompare TableCompare.cpp $(SRC)

TableConvert: $(DEPS) $(SRC) TableConvert.cpp
	g++ -O2 -o TableConvert TableConvert.cpp $(SRC)

TableDump: $(DEPS) $(SRC) TableDump.cpp
	g++ -O2 -o TableDump TableDump.cpp $(SRC)


clean:
	rm -f TableCompare TableConvert TableDump
