
TARGET		:=	$(notdir $(CURDIR))
SOURCES		:=	source
INCLUDES	:=	include

CPPFILES	:=	$(foreach dir,$(SOURCES),$(dir)/$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S))) $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
INCLUDE		:=	$(foreach dir,$(INCLUDES), -iquote $(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include)

ifeq ($(UNAME),Linux)
OUTPUT := $(TARGET)
else
OUTPUT := $(TARGET)
endif

.PHONY: all clean

$(OUTPUT):
	@echo building $(OUTPUT)...
	@g++ $(INCLUDE) $(CPPFILES) $(SFILES) -o $(OUTPUT)

all: $(OUTPUT)
clean:
	@echo cleaning...
	@rm -fr $(OUTPUT) loader.bin