plugin_LTLIBRARIES = libgstusb.la

libgstusb_la_SOURCES = gstplugin.c \
gstusbsink.c gstusbsink.h \
gstusbsrc.c gstusbsrc.h \
usbgadget.c usbgadget.h \
usbstring.c usbstring.h \
usbhost.c usbhost.h \
usbgadget_descriptors.h


# compiler and linker flags used to compile this plugin, set in configure.ac
libgstusb_la_CFLAGS = $(GST_CFLAGS) $(LIBUSB_CFLAGS)
libgstusb_la_LIBADD = $(GST_LIBS) $(LIBUSB_LIBS)
libgstusb_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstusb_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstusbsrc.h gstusbsink.h usbstring.h usbhost.h usbgadget.h\
 usbgadget_descriptors.h


clean-local:
	-rm -rf cmem