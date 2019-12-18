#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPPC)/wii_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
TARGET		:=	$(notdir $(CURDIR))
BUILD		:=	build
SOURCES		:=	source
LDR_SOURCES	:=	loader
DATA		:=	data
INCLUDES	:=  include
ENTRYPOINT	:=	0x81100000

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------

CFLAGS	= -g -O2 -Wall $(MACHDEP) $(INCLUDE)
CXXFLAGS	=	$(CFLAGS)

LDFLAGS	=	-g $(MACHDEP) -Wl,-Map,$(notdir $@).map -Wl,--section-start,.init=$(ENTRYPOINT)

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:=	-lwiikeyboard -ldb -lwiiuse -lbte -logc -lfat

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=

#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

#---------------------------------------------------------------------------------
# automatically build a list of object files for our project
#---------------------------------------------------------------------------------
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
LDRFILES	:=	$(foreach dir,$(LDR_SOURCES),$(wildcard $(dir)/*.c)) \
				$(foreach dir,$(LDR_SOURCES),$(wildcard $(dir)/*.cpp)) \
				$(foreach dir,$(LDR_SOURCES),$(wildcard $(dir)/*.S))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) \
					$(sFILES:.s=.o) $(SFILES:.S=.o)

#---------------------------------------------------------------------------------
# build a list of include paths
#---------------------------------------------------------------------------------
export INCLUDE	:=	$(foreach dir,$(INCLUDES), -iquote $(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD) \
					-I$(LIBOGC_INC)

#---------------------------------------------------------------------------------
# build a list of library paths
#---------------------------------------------------------------------------------
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
					-L$(LIBOGC_LIB)

export OUTPUT	:=	$(CURDIR)/$(TARGET)
.PHONY: build clean pre_build loader build_priiloader post_build
#---------------------------------------------------------------------------------
#lets see what OS we are on
UNAME := $(shell uname)
NUMBER := $(shell bash -c 'echo $$RANDOM')
#build OpenDolBoot
ifeq ($(UNAME),Linux)
OPENDOLBOOT := "OpenDolBoot"
else
OPENDOLBOOT := "OpenDolBoot.exe"
endif

#build order
all: $(BUILD) run

#actual build script
$(BUILD):
	@make --no-print-directory -f $(CURDIR)/Makefile loader 
	@make --no-print-directory -f $(CURDIR)/Makefile build_priiloader 
	@make --no-print-directory -f $(CURDIR)/Makefile post_build

pre_build:
	@echo
	@echo -----------------------------------------------
	@echo preparing compilation of priiloader...
	@echo
ifeq ($(wildcard $(DATA)/certs.bin),)
	@echo "wii-cert is missing (certs.bin)."
	@echo "please add the Wii XS3 certificate in the data directory"
	@echo "This file can be found in /sys/cert.sys in your Wii NAND"
	@echo "just copy it to /data/ and rename it to certs.bin"
	@echo
	@exit 1
endif

ifeq ($(CI_BUILD),)
ifneq ($(shell md5sum $(DATA)/certs.bin | cut -d\  -f1),7ff50e2733f7a6be1677b6f6c9b625dd)
	@echo "Looks like your certs.bin is invalid. "
	@echo "It's supposed to have the MD5 checksum 7ff50e2733f7a6be1677b6f6c9b625dd"
	@echo "Please grab a new copy of the cert.sys from your Wii NAND"
	@echo 
	@exit 1
endif
endif

ifeq ($(wildcard ./$(OPENDOLBOOT)),)
	@g++ ../tools/OpenDolBoot/source/OpenDolBoot.cpp -o $(OPENDOLBOOT)
endif

ifeq ($(wildcard $(DATA)/loader.bin),)
	@echo "loader is missing (loader.bin)."
	@echo "please run 'make clean' followed by 'make' to recompile priiloader"
	@echo
	@exit 1
endif

ifeq ($(wildcard ../.git/.),)
	@echo
	@echo
	@echo !!!!!!WARNING!!!!!!!
	@echo --------------------
	@echo NOT BUILDING FROM GIT CHECKOUT. REV NUMBERS WILL NOT MAKE ANY SENSE
	@echo
	@echo
	@echo '#define GIT_REV 0x'$(NUMBER) > ../Shared/gitrev.h
	@echo '#define GIT_REV_STR "0x'$(NUMBER)'-no-git"' >> ../Shared/gitrev.h
	@echo 'Using 0x'$(NUMBER)' as revision, '0x'$(NUMBER)'-no-git as revision string'
else
	@echo "#define GIT_REV 0x$(shell git describe --always --long)" > ../Shared/gitrev.h
	@echo "#define GIT_REV_STR \"0x$(shell git describe --always --long --dirty)\"" >> ../Shared/gitrev.h
	@echo "Using 0x$(shell git describe --always --long) as revision, \"0x$(shell git describe --always --long --dirty)\" as revision string"
endif

#test if the build dir exists, make it if its not
	@test -d $(BUILD) || mkdir $(BUILD)

loader: $(DATA)/loader.bin
$(DATA)/loader.bin: $(LDRFILES)
	@echo -----------------------------------------------
	@echo compiling the loader...
	@echo -----------------------------------------------
	@echo 
#test if the build dir exists, make it if its not
	@test -d $(BUILD) || mkdir $(BUILD)
#the generated ASM by the following command has no purpose but for debugging needs.
	@echo generating loader.s
	@$(CC) --static -iquote include -I $(BUILD) -I$(LIBOGC_INC) -S loader/loader.c -o $(BUILD)/loader.s
#these ones however do the actual compiling & converting to binary
	@echo loader.c
	@$(CC) --static -iquote include -I $(BUILD) -I$(LIBOGC_INC) -c loader/loader.c -o $(BUILD)/loader.o
	@echo linking loader.bin	
	@$(PREFIX)ld $(BUILD)/loader.o -o $(DATA)/loader.bin --oformat=binary --entry=_start --static
	
build_priiloader: pre_build loader
	@echo
	@echo -----------------------------------------------
	@echo compiling priiloader...	
	@echo 
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

post_build:
	@echo -----------------------------------------------
	@echo creating priiloader.app for installer
ifeq ($(UNAME),Linux)
		@echo
		@./OpenDolBoot $(OUTPUT).dol ../Installer/data/priiloader.app
else
		@echo
		@./OpenDolBoot.exe $(OUTPUT).dol ../Installer/data/priiloader.app
endif

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol $(DATA)/loader.bin

#---------------------------------------------------------------------------------
run:
#send elf to the wii
ifneq ($(WIILOAD),)
	@echo -----------------------------------------------
	@echo loading $(TARGET).elf
	wiiload $(TARGET).elf
else
	@echo "wiiload variable not set. skipping sending to homebrew channel (wiiload)"
endif

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

#---------------------------------------------------------------------------------
# This rule links in binary data with the .bin,tik & tmd extension
#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	$(bin2o)
	
-include $(DEPENDS)

%.tik.o	:	%.tik
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	$(bin2o)

-include $(DEPENDS)

%.tmd.o	:	%.tmd
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
