ifeq ($(wildcard ../../.git/.),)
export NUMBER 		:= $(shell bash -c 'echo $$RANDOM')
export NUMBERSTR	:= $(NUMBER)-dirty
else
export NUMBER		:= $(shell git rev-parse --verify --short HEAD)
export NUMBERSTR	:= $(shell git describe --always --long)
endif

export VERSIONLINE	:= \#define GIT_REV 0x$(NUMBER)
export VERSIONSTR	:= \#define GIT_REV_STR \"$(NUMBERSTR)\"
ifneq ($(wildcard $(SHARED)/gitrev.h),)
export READVERSION	:= $(shell head -n 1 $(SHARED)/gitrev.h)
endif

GenerateVersion:
ifeq ($(wildcard ../../.git/.),)
	$(SILENTMSG)
	$(SILENTMSG)
	$(SILENTMSG) !!!!!!WARNING!!!!!!!
	$(SILENTMSG) --------------------
	$(SILENTMSG) NOT BUILDING FROM GIT CHECKOUT. REV NUMBERS WILL NOT MAKE ANY SENSE
	$(SILENTMSG)
	$(SILENTMSG)
endif

ifneq ($(VERSIONLINE),$(READVERSION))
	$(SILENTMSG) writing versions to file
	$(SILENTMSG) "$(VERSIONLINE)" > $(SHARED)/gitrev.h
	$(SILENTMSG) "$(VERSIONSTR)" >> $(SHARED)/gitrev.h
endif
	$(SILENTMSG) "Using 0x$(NUMBER) as revision, $(NUMBERSTR) as revision string"