all:
	@cd priiloader && $(MAKE) all
	@cd Installer && $(MAKE) all

clean:
	@cd priiloader && $(MAKE) clean
	@cd Installer && $(MAKE) clean
	 
