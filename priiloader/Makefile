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
SOURCES		:=	source ../Shared
LOADER		:=  ../loader
DATA		:=	data
INCLUDES	:=  include ../Shared ../loader/include
#---------------------------------------------------
# note : do not change entrypoint without testing booting SM, dol/elf &nd game content.
# it will fuck one or the other up
ENTRYPOINT	:=	0x81330000

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
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*))) 
LOADERFILE	:=	loader.bin

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(addsuffix .o,$(LOADERFILE)) \
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

.PHONY: $(BUILD) clean rebuild pre_build post_build
#---------------------------------------------------------------------------------
#lets see what OS we are on
UNAME := $(shell uname)
NUMBER := $(shell bash -c 'echo $$RANDOM')
OPENDOLBOOT	:= ../tools/OpenDolBoot
BOOTLOADER	:= ../tools/bootloader
#build OpenDolBoot
ifeq ($(UNAME),Linux)
export OPENDOLBOOT	:= $(OPENDOLBOOT)/OpenDolBoot
export BOOTLOADER	:= $(BOOTLOADER)/bootloader
else
export OPENDOLBOOT	:= $(OPENDOLBOOT)/OpenDolBoot.exe
export BOOTLOADER	:= $(BOOTLOADER)/bootloader.exe
endif

#build order
all: $(BUILD) run

#actual build script
$(BUILD): pre_build
	@echo
	@echo -----------------------------------------------
	@echo compiling priiloader...	
	@echo 
	@[ -d $@ ] || mkdir -p $@
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
	@make --no-print-directory -f $(CURDIR)/Makefile post_build

pre_build:
ifeq ($(wildcard $(OPENDOLBOOT)),)
	@echo "opendolboot is missing $(OPENDOLBOOT)."
	@echo "please run 'make clean' followed by 'make' in the root directory to rebuild priiloader"
	@echo
	@exit 1
endif

ifeq ($(wildcard $(BOOTLOADER)),)
	@echo "bootloader is missing $(BOOTLOADER)."
	@echo "please run 'make clean' followed by 'make' in the root directory to rebuild priiloader"
	@echo
	@exit 1
endif

ifeq ($(wildcard $(LOADER)/loader.bin),)
	@echo "loader is missing (loader.bin)."
	@echo "please run 'make clean' followed by 'make' in the root directory to rebuild priiloader"
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
	@echo '#define GIT_REV_STR "'$(NUMBER)'-no-git"' >> ../Shared/gitrev.h
	@echo 'Using 0x'$(NUMBER)' as revision, ''$(NUMBER)'-no-git as revision string'
else
	@echo "#define GIT_REV 0x$(shell git rev-parse --verify --short HEAD)" > ../Shared/gitrev.h
	@echo "#define GIT_REV_STR \"$(shell git describe --always --long --dirty)\"" >> ../Shared/gitrev.h
	@echo "Using 0x$(shell git rev-parse --verify --short HEAD) as revision, \"$(shell git describe --always --long --dirty)\" as revision string"
endif

post_build:
	@echo -----------------------------------------------
	@echo creating priiloader.app for installer
	@echo
	@./OpenDolBoot $(OUTPUT).dol ../Installer/data/priiloader.app

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol $(OUTPUT)_hbc.dol $(DATA)/loader.bin $(OPENDOLBOOT) $(BOOTLOADER)
	
rebuild : clean $(BUILD)

#---------------------------------------------------------------------------------
run:
#send elf to the wii
ifneq ($(WIILOAD),)
	@echo -----------------------------------------------
	@echo loading $(OUTPUT)_hbc.dol
	@wiiload $(OUTPUT)_hbc.dol
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
# This rule links in the loader project
#---------------------------------------------------------------------------------
loader: loader.bin.o
loader.bin.o : ../$(LOADER)/loader.bin #this path is relative from the a path in $(SOURCES)
	@echo $(notdir $<)
	$(bin2o)

#---------------------------------------------------------------------------------
# This rule links in binary data with the .bin,tik & tmd extension
#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	$(bin2o)

%.tik.o	:	%.tik
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	$(bin2o)

%.tmd.o	:	%.tmd
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
