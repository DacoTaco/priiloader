all:
	$(MAKE) -C loader all
	$(MAKE) -C priiloader all
	$(MAKE) -C Installer all

clean:
	$(MAKE) -C loader clean
	$(MAKE) -C priiloader clean
	$(MAKE) -C Installer clean
	 
