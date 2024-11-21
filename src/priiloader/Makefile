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
TARGET			:=	$(notdir $(CURDIR))
BUILD			:=	build
SHARED			:=	../Shared
SOURCES			:=	source $(SHARED)
LOADER			:=  ../loader
DATA			:=	data
INCLUDES		:=  include $(SHARED) ../loader/include
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
LIBS	:=	-lwiikeyboard -ldb -lwiiuse -lbte -lfat -logc

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

export OUTPUT		:=	$(CURDIR)/$(TARGET)
export BASE_OUTPUT	:=	$(CURDIR)/$(TARGET)_base

export VPATH		:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
						$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR		:=	$(CURDIR)/$(BUILD)

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

.PHONY: $(BUILD) clean rebuild pre_build post_build GenerateVersion
#---------------------------------------------------------------------------------
#lets see what OS we are on
UNAME := $(shell uname)
OPENDOLBOOT	:= ../../tools/OpenDolBoot
BOOTLOADER	:= ../../tools/bootloader
#build OpenDolBoot
ifeq ($(UNAME),Linux)
export OPENDOLBOOT	:= $(OPENDOLBOOT)/OpenDolBoot
export BOOTLOADER	:= $(BOOTLOADER)/bootloader
else ifeq ($(UNAME),Darwin)
export OPENDOLBOOT	:= $(OPENDOLBOOT)/OpenDolBoot
export BOOTLOADER	:= $(BOOTLOADER)/bootloader
else
export OPENDOLBOOT	:= $(OPENDOLBOOT)/OpenDolBoot.exe
export BOOTLOADER	:= $(BOOTLOADER)/bootloader.exe
endif

#build order
all: $(BUILD) run

include $(SHARED)/GitVersion.mk

#actual build script
$(BUILD): pre_build
	$(SILENTMSG)
	$(SILENTMSG) -----------------------------------------------
	$(SILENTMSG) compiling priiloader...	
	$(SILENTMSG) 
	@[ -d $@ ] || mkdir -p $@
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
	@make --no-print-directory -f $(CURDIR)/Makefile post_build

pre_build: GenerateVersion
ifeq ($(wildcard $(OPENDOLBOOT)),)
	$(SILENTMSG) "opendolboot is missing $(OPENDOLBOOT)."
	$(SILENTMSG) "please run 'make clean' followed by 'make' in the root directory to rebuild priiloader"
	$(SILENTMSG)
	@exit 1
endif

ifeq ($(wildcard $(BOOTLOADER)),)
	$(SILENTMSG) "bootloader is missing $(BOOTLOADER)."
	$(SILENTMSG) "please run 'make clean' followed by 'make' in the root directory to rebuild priiloader"
	$(SILENTMSG)
	@exit 1
endif

ifeq ($(wildcard $(LOADER)/loader.bin),)
	$(SILENTMSG) "loader is missing (loader.bin)."
	$(SILENTMSG) "please run 'make clean' followed by 'make' in the root directory to rebuild priiloader"
	$(SILENTMSG)
	@exit 1
endif

post_build:
	$(SILENTMSG) -----------------------------------------------
	$(SILENTMSG) creating priiloader.app for installer
	$(SILENTMSG)
	$(SILENTCMD) $(OPENDOLBOOT) $(OUTPUT).dol ../Installer/data/priiloader.app

#---------------------------------------------------------------------------------
clean:
	$(SILENTMSG) clean ...
	@rm -fr $(BUILD) $(BASE_OUTPUT).dol $(OUTPUT).elf $(OUTPUT).dol $(DATA)/loader.bin
	
rebuild : clean $(BUILD)

#---------------------------------------------------------------------------------
run:
#send elf to the wii
ifneq ($(WIILOAD),)
	$(SILENTMSG) -----------------------------------------------
	$(SILENTMSG) loading $(OUTPUT).dol
	@wiiload $(OUTPUT).dol
else
	$(SILENTMSG) "wiiload variable not set. skipping sending to homebrew channel (wiiload)"
endif

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).dol: $(BASE_OUTPUT).dol
	$(SILENTMSG) creating $(notdir $@)
	$(SILENTCMD) ../$(BOOTLOADER) $< $@
$(BASE_OUTPUT).dol: $(BASE_OUTPUT).elf
$(BASE_OUTPUT).elf: $(OFILES)

#---------------------------------------------------------------------------------
# This rule links in the loader project
#---------------------------------------------------------------------------------
loader: loader.bin.o
loader.bin.o : ../$(LOADER)/loader.bin #this path is relative from the a path in $(SOURCES)
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

#---------------------------------------------------------------------------------
# This rule links in binary data with the .bin,tik & tmd extension
#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
#---------------------------------------------------------------------------------
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

%.tik.o	:	%.tik
#---------------------------------------------------------------------------------
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

%.tmd.o	:	%.tmd
#---------------------------------------------------------------------------------
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
