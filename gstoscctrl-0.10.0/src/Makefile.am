# Note: plugindir is set in configure

plugin_LTLIBRARIES = libgstcontrolfromosc.la

# sources used to compile this plug-in
libgstcontrolfromosc_la_SOURCES = \
    gstcontrolfromosc.c \
    gstcontrolfromosc.h \
    aux_functions.c \
    aux_functions.h \
    osc_handlers.c \
    osc_handlers.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstcontrolfromosc_la_CFLAGS = \
    $(GST_CFLAGS) \
    $(LIBLO_CFLAGS)

libgstcontrolfromosc_la_LIBADD = \
    $(GST_LIBS)    

libgstcontrolfromosc_la_LDFLAGS = \
    $(GST_PLUGIN_LDFLAGS) \
    $(LIBLO_LDFLAGS)

libgstcontrolfromosc_la_LIBTOOLFLAGS = \
    --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = \
    gstcontrolfromosc.h \
    osc_handlers.h
