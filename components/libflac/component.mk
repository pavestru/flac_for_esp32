#
# Component Makefile
#
COMPONENT_ADD_INCLUDEDIRS := include include/FLAC include/share include/private include/protected

COMPONENT_SRCDIRS := library library/flac library/libFLAC

CFLAGS += -Wno-unused-function -DHAVE_CONFIG_H -Os -DSMALL_FOOTPRINT -funroll-loops -ffast-math
