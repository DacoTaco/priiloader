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
DATA		:=	data
INCLUDES	:=  include


#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)

#---------------------------------------------------------------------------------
# automatically build a list of object files for our project
#---------------------------------------------------------------------------------
CFILES				:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES			:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES				:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES				:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES			:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))					
export SOURCE		:=	$(foreach file, $(CFILES), ../$(SOURCES)/$(file)) \
						$(foreach file, $(CPPFILES), ../$(SOURCES)/$(file)) \
						$(foreach file, $(sFILES), ../$(SOURCES)/$(file)) \
						$(foreach file, $(SFILES), ../$(SOURCES)/$(file)) \
						$(foreach file, $(BINFILES), ../$(SOURCES)/$(file)) 

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(sFILES:.s=.o) $(SFILES:.S=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES := $(addsuffix .h,$(subst .,_,$(BINFILES)))

#---------------------------------------------------------------------------------
# build a list of include paths
#---------------------------------------------------------------------------------
export INCLUDE	:=	$(foreach dir,$(INCLUDES), -iquote $(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD) \
					-I$(LIBOGC_INC)
					
.PHONY: $(BUILD) clean loader

#---------------------------------------------------------------------------------
all: $(BUILD)
$(BUILD): 
	@[ -d $@ ] || mkdir -p $@
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile loader

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).bin

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)
-include $(DEPENDS)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
.PHONY: loader

loader: $(OUTPUT).bin
$(OUTPUT).bin: $(SOURCEFILES) 
	@echo -----------------------------------------------
	@echo compiling the loader...
	@echo -----------------------------------------------
	@echo 
	@echo building $(notdir $@) ...	
	$(CC) $(foreach src, $(SOURCE), $(src)) --save-temps --static $(INCLUDE) -O2 -fno-asynchronous-unwind-tables -nostartfiles -nodefaultlibs -Wl,-Map,loader.map -Wl,--entry=_start -Wl,--oformat=binary -o $@
	
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------