all:
	$(MAKE) -C loader
	$(MAKE) -C ./tools/bootloader
	$(MAKE) -C ./tools/OpenDolBoot
	$(MAKE) -C priiloader
	$(MAKE) -C Installer

clean:
	$(MAKE) -C ./tools/bootloader clean
	$(MAKE) -C ./tools/OpenDolBoot clean
	$(MAKE) -C loader clean
	$(MAKE) -C priiloader clean
	$(MAKE) -C Installer clean
