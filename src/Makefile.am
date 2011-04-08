plugin_LTLIBRARIES = libgstusb.la

libgstusb_la_SOURCES = gstplugin.c \
gstusbsink.c gstusbsink.h \
gstusbsrc.c gstusbsrc.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstusb_la_CFLAGS = $(GST_CFLAGS)
libgstusb_la_LIBADD = $(GST_LIBS)
libgstusb_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstusb_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstusbsrc.h gstusbsink.h
