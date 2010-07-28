
INCLUDES = -I/usr/local/atical/include
LIBS = -laticalrt -laticalcl

CPP_FLAGS = -O2 -fPIC

all: A5Brook.so

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

A5Brook.o: A5Brook.cpp A5Brook.h A5Slice.h CalDevice.o CalModule.o
	gcc $(CPP_FLAGS) -c A5Brook.cpp $(INCLUDES) -D BUILDING_DLL

A5Brook.so: A5Slice.o A5Brook.o CalDevice.o CalModule.o kernelLib.o \
	my_kernel_dp11.o my_kernel_dp12.o my_kernel_dp13.o my_kernel_dp14.o
	g++ -o A5Brook.so A5Brook.o A5Slice.o CalDevice.o \
	my_kernel_dp11.o my_kernel_dp12.o my_kernel_dp13.o \
	my_kernel_dp14.o CalModule.o kernelLib.o $(LIBS) -ldl -lz -shared

CalDevice.o: CalDevice.cpp CalDevice.h
	gcc $(CPP_FLAGS) -c CalDevice.cpp $(INCLUDES)

CalModule.o: CalModule.cpp CalModule.h CalDevice.h 
	gcc $(CPP_FLAGS) -c CalModule.cpp $(INCLUDES)

clean:
	rm -rf CalDevice.o CalModule.o A5Brook.o A5Slice.o \
	A5Brook.so kernelLib.o my_kernel_dp*.o

