all:
	@cd priiloader && $(MAKE) all
	@cd installer && $(MAKE) all

clean:
	@cd priiloader && $(MAKE) clean
	@cd installer && $(MAKE) clean
	 