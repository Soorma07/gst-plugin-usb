include @RULES_MAKE@

plugin_LTLIBRARIES = libgstusb.la

libgstusb_la_SOURCES = gstplugin.c \
gstusbsink.c gstusbsink.h \
gstusbsrc.c gstusbsrc.h \
usbgadget.c usbgadget.h \
usbstring.c usbstring.h \
usbhost.c usbhost.h \
usbgadget_descriptors.h\
gstusbclock.c gstusbclock.h


# compiler and linker flags used to compile this plugin, set in configure.ac
libgstusb_la_CFLAGS = $(GST_CFLAGS) $(LIBUSB_CFLAGS) $(shell cat cmem/compiler.opt)
libgstusb_la_LIBADD = $(GST_LIBS) $(LIBUSB_LIBS)
libgstusb_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -Wl,cmem/linker.cmd 
libgstusb_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstusbsrc.h gstusbsink.h usbstring.h usbhost.h usbgadget.h\
 usbgadget_descriptors.h gstusbclock.h
EXTRA_DIST = *.cfg

#################################################
# Delete this section if you are not interested #
# in allocation memory with CMEM                #
#################################################
# XDC Configuration
CONFIGURO     = $(XDC_INSTALL_DIR)/xs xdc.tools.configuro
BUILT_SOURCES = cmem/compiler.opt cmem/linker.cmd
export XDCPATH  = $(XDC_INSTALL_DIR)/packages;$(LINK_INSTALL_DIR);$(FC_INSTALL_DIR)/packages;$(CE_INSTALL_DIR)/packages;$(XDAIS_INSTALL_DIR)/packages;$(CODEC_INSTALL_DIR)/packages;$(CMEM_INSTALL_DIR)/packages;$(DMAI_INSTALL_DIR)/packages;$(LPM_INSTALL_DIR)/packages;$(BIOS_INSTALL_DIR)/packages;

XDC_PLATFORM=undefined_xdc_platform
XDC_TARGET=undefined_xdc_target
export CSTOOL_PREFIX

include @PLATFORMDEFS@

%/compiler.opt %/linker.cmd : %.cfg
	$(CONFIGURO) -c $(CSTOOL_PATH) -o cmem -t $(XDC_TARGET) -p $(XDC_PLATFORM) -b config.bld $<

#################################################
#              End of CMEM section              #
#################################################


clean-local:
	-rm -rf cmem