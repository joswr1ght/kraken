all:
	$(MAKE) -C a5_ati
	cd a5_cpu && ./build.sh
	cd Kraken && ./build.sh
	$(MAKE) -C TableConvert
	$(MAKE) -C TableGeneration
	$(MAKE) -C Utilities

noati:
	cd a5_cpu && ./build.sh
	cd Kraken && ./build.sh
	$(MAKE) -C TableConvert
	$(MAKE) -C Utilities

a5_ati:
	$(MAKE) -C a5_ati

a5_cpu:
	cd a5_cpu && ./build.sh

Kraken:
	cd Kraken && ./build.sh

TableConvert:
	$(MAKE) -C TableConvert

TableGeneration:
	$(MAKE) -C TableGeneration

Utilities:
	$(MAKE) -C Utilities

clean:
	$(MAKE) -C TableConvert clean
	$(MAKE) -C TableGeneration clean
	$(MAKE) -C Utilities clean
	$(MAKE) -C a5_ati clean
	@rm ./a5_cpu/A5Cpu.so ./a5_cpu/a5cpu_test ./Kraken/kraken ./Kraken/A5Cpu.so 

noaticlean:
	$(MAKE) -C TableConvert clean
	$(MAKE) -C Utilities clean
	@rm ./a5_cpu/A5Cpu.so ./a5_cpu/a5cpu_test ./Kraken/kraken ./Kraken/A5Cpu.so 
