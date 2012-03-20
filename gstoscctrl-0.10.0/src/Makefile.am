# Note: plugindir is set in configure

plugin_LTLIBRARIES = libgstoscctrl.la

# sources used to compile this plug-in
libgstoscctrl_la_SOURCES = \
    gstoscctrl.c \
    gstoscctrl.h \
    aux_functions.c \
    aux_functions.h \
    osc_handlers.c \
    osc_handlers.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstoscctrl_la_CFLAGS = \
    $(GST_CFLAGS) \
    $(LIBLO_CFLAGS)

libgstoscctrl_la_LIBADD = \
    $(GST_LIBS)    

libgstoscctrl_la_LDFLAGS = \
    $(GST_PLUGIN_LDFLAGS) \
    $(LIBLO_LDFLAGS)

libgstoscctrl_la_LIBTOOLFLAGS = \
    --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = \
    gstoscctrl.h \
    osc_handlers.h
