TARGET		:=	$(notdir $(CURDIR))
SOURCES		:=	source
LOADERPATH	:=	../../loader/loader.bin

BIN2S 		:= bin2s
UNAME 		:= $(shell uname)
CPPFILES 	:= $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
SFILES      := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.S)) $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.s))
LOADER_SFILE:= $(SOURCES)/loader.bin.s
LOADER_HFILE:= $(SOURCES)/loader.bin.h

ifeq ($(UNAME),Linux)
  OUTPUT := $(TARGET)
else ifeq ($(UNAME),Darwin)
  OUTPUT := $(TARGET)
  BIN2S_FLAGS  := --apple-llvm
else
  OUTPUT := $(TARGET).exe
endif

.PHONY: clean

all: $(OUTPUT)

$(LOADER_SFILE): $(LOADERPATH)
	@$(BIN2S) $(BIN2S_FLAGS) -H $(LOADER_HFILE) $< > $@

$(OUTPUT): $(CPPFILES) $(SFILES) $(LOADER_SFILE)
	@g++ $^ -o $@

clean:
	@echo cleaning...
	@rm -fr $(OUTPUT) $(LOADER_SFILE) $(LOADER_HFILE)

