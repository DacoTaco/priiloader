all:
	$(MAKE) -C ./src/loader
	$(MAKE) -C ./tools/bootloader
	$(MAKE) -C ./tools/OpenDolBoot	
	$(MAKE) -C ./src/priiloader
	$(MAKE) -C ./src/Installer

clean:
	$(MAKE) -C ./tools/bootloader clean
	$(MAKE) -C ./tools/OpenDolBoot clean
	$(MAKE) -C ./src/loader clean
	$(MAKE) -C ./src/priiloader clean
	$(MAKE) -C ./src/Installer clean
