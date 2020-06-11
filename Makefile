all:
	$(MAKE) -C loader
	$(MAKE) -C priiloader
	$(MAKE) -C Installer

clean:
	$(MAKE) -C loader clean
	$(MAKE) -C priiloader clean
	$(MAKE) -C Installer clean
	 
