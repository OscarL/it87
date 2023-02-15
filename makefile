## BeOS Generic Makefile v2.2 ##

NAME= it87
TYPE= DRIVER

SRCS= it87_driver.c
RSRCS= 

LIBS=
LIBPATHS= 
SYSTEM_INCLUDE_PATHS = 
LOCAL_INCLUDE_PATHS = 
OPTIMIZE= FULL
DEFINES= 
WARNINGS = ALL
SYMBOLS = 
DEBUGGER = 
#COMPILER_FLAGS = -Wno-unused
LINKER_FLAGS =
APP_VERSION = 
DRIVER_PATH = misc

## include the makefile-engine
include $(BUILDHOME)/etc/makefile-engine

CFLAGS := $(filter-out -Wno-ctor-dtor-privacy,$(CFLAGS))
