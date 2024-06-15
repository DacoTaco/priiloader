TARGET		:=	$(notdir $(CURDIR))
SOURCES		:=	source
INCLUDES	:=	include
NANDLDR		:=	nandloader
BUILD		:=	build

HOST		:=	$(shell uname)
MINGW_AVAIL	:=	$(shell command -v x86_64-w64-mingw32-g++)
NANDLDR_BIN	:=	$(NANDLDR).bin
NANDLDR_PATH:=	$(NANDLDR)/$(NANDLDR_BIN)
NANDLDR_SSRC:= 	$(SOURCES)/$(NANDLDR_BIN).S
NANDLDR_HSRC:= 	$(INCLUDES)/$(NANDLDR_BIN).h
CFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.c))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
sFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.s))
SFILES		:=	$(foreach dir,$(SOURCES),$(wildcard $(dir)/*.S))
INCLUDE		:=	$(foreach dir,$(INCLUDES), -iquote $(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include)

#default commands/settings
SILENTMSG	=	@echo 
BIN2S 		=	bin2s
CFLAGS		=	-Os -s
CXXFLAGS	= 	-std=c++14
CC			=	$(PREFIX)gcc
CXX			=	$(PREFIX)g++
LD			=	$(PREFIX)ld
ifeq ($(OUTPUT),)
OUTPUT		:=	$(TARGET)
endif

#for everything except mac, we will try and build a single, statically linked exe
#macos doesn't support it : https://stackoverflow.com/questions/5259249/creating-static-mac-os-x-c-build
ifneq ($(HOST),Darwin)
CFLAGS		+=	-static -static-libstdc++ -static-libgcc
else
BIN2S_FLAGS	:= --apple-llvm
endif

#windows, msys, mingw,...
ifeq ($(HOST), WIN32)
OUTPUT 		:=	$(TARGET).exe
CFLAGS		+=	-DWIN32
ifneq ($(MINGW_AVAIL),)
PREFIX		:=	x86_64-w64-mingw32-
CFLAGS		+=	-lws2_32
endif
endif

.PHONY: all clean
all: $(OUTPUT)

clean:
	@echo cleaning...
	@make --no-print-directory -C $(NANDLDR) clean
	@rm -fr $(OUTPUT) $(BUILD) $(NANDLDR_SSRC) $(NANDLDR_HSRC)

$(NANDLDR_PATH): 
	@make --no-print-directory -C $(NANDLDR)

$(NANDLDR_SSRC): $(NANDLDR_PATH)
	@$(BIN2S) $(BIN2S_FLAGS) -H $(NANDLDR_HSRC) $< > $@

$(OUTPUT): $(CPPFILES) $(CFILES) $(SFILES) $(NANDLDR_SSRC)
	@echo $@ ...
	@$(CXX) $(INCLUDE) $^ -o $@ $(CFLAGS) $(CXXFLAGS)