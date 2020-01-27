all:
	$(MAKE) -C priiloader all
	$(MAKE) -C Installer all

clean:
	$(MAKE) -C priiloader clean
	$(MAKE) -C Installer clean
	 
