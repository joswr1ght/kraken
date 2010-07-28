
INCLUDES = -I/usr/local/atical/include
LIBS = -laticalrt -laticalcl

CPP_FLAGS = -O2 -fPIC

all: A5Ati.so

my_kernel_dp11.o: my_kernel_dp11.Z
	ld -s -r -o my_kernel_dp11.o -b binary my_kernel_dp11.Z

my_kernel_dp12.o: my_kernel_dp12.Z
	ld -s -r -o my_kernel_dp12.o -b binary my_kernel_dp12.Z

my_kernel_dp13.o: my_kernel_dp13.Z
	ld -s -r -o my_kernel_dp13.o -b binary my_kernel_dp13.Z

my_kernel_dp14.o: my_kernel_dp14.Z
	ld -s -r -o my_kernel_dp14.o -b binary my_kernel_dp14.Z

A5Slice.o: A5Slice.cpp A5Slice.h
	gcc $(CPP_FLAGS) -c A5Slice.cpp $(INCLUDES)

kernelLib.o: kernelLib.cpp kernelLib.h
	gcc $(CPP_FLAGS) -c kernelLib.cpp $(INCLUDES)

Advance.o: Advance.h Advance.cpp
	gcc $(CPP_FLAGS) -c Advance.cpp $(INCLUDES) -D BUILDING_DLL

A5Ati.o: A5Ati.cpp A5Ati.h A5Slice.h CalDevice.o CalModule.o Advance.h
	gcc $(CPP_FLAGS) -c A5Ati.cpp $(INCLUDES) -D BUILDING_DLL

A5Ati.so: A5Slice.o A5Ati.o CalDevice.o CalModule.o kernelLib.o \
	my_kernel_dp11.o my_kernel_dp12.o my_kernel_dp13.o my_kernel_dp14.o \
	Advance.o
	g++ -o A5Ati.so A5Ati.o A5Slice.o CalDevice.o Advance.o \
	my_kernel_dp11.o my_kernel_dp12.o my_kernel_dp13.o \
	my_kernel_dp14.o CalModule.o kernelLib.o $(LIBS) -ldl -lz -shared

CalDevice.o: CalDevice.cpp CalDevice.h
	gcc $(CPP_FLAGS) -c CalDevice.cpp $(INCLUDES)

CalModule.o: CalModule.cpp CalModule.h CalDevice.h 
	gcc $(CPP_FLAGS) -c CalModule.cpp $(INCLUDES)

clean:
	rm -rf CalDevice.o CalModule.o A5Ati.o A5Slice.o \
	A5Ati.so Advance.o kernelLib.o my_kernel_dp*.o

