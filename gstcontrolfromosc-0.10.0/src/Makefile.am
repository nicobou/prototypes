# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstplugin.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstcontrolfromosc.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in
libgstcontrolfromosc_la_SOURCES = gstcontrolfromosc.c gstcontrolfromosc.h \
                                  aux_functions.c aux_functions.h \
                                  osc_handlers.c osc_handlers.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstcontrolfromosc_la_CFLAGS = $(GST_CFLAGS)
libgstcontrolfromosc_la_LIBADD = $(GST_LIBS)
libgstcontrolfromosc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstcontrolfromosc_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstcontrolfromosc.h osc_handlers.h
