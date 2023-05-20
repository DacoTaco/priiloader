TARGET		:=	$(notdir $(CURDIR))
SOURCES		:=	.
LOADERPATH	:=	../../loader/loader.bin

BIN2S := bin2s
UNAME := $(shell uname)

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

loader.bin.s: $(LOADERPATH)
	@$(BIN2S) $(BIN2S_FLAGS) -H loader.bin.h $< > $@

$(OUTPUT): bootloader.cpp loader.bin.s
	@g++ $^ -o $@

clean:
	@echo cleaning...
	@rm -fr $(OUTPUT) loader.bin.s loader.bin.h

