# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstplugin.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstplugin.la

EXTRA_FLAGS=-I ../../lib/igb/
EXTRA_FLAGS+=-L ../../lib/igb/ -ligb -lpci -lrt -lpthread -lm  -lgstapp-1.0

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in

#For Talker uncomment the below:

#libgstplugin_la_SOURCES = gstavbsink.c gstavbsink.h ../../../common/avb.c ../../../common/avb.h ../../../../lib/igb/igb.h


#For Listener uncomment the below:

#libgstplugin_la_SOURCES = gst_avb_playbin.c gst_avb_playbin.h ../../../common/avb.c ../../../common/avb.h ../../../../lib/igb/igb.h gst_app_src.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstplugin_la_CFLAGS = $(GST_CFLAGS) 
libgstplugin_la_LIBADD = $(GST_LIBS)  
libgstplugin_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(LDFLAGS)
libgstplugin_la_LIBTOOLFLAGS = --tag=disable-static
libgstplugin_la_EXTRA_FLAGS=$(EXTRA_FLAGS)


# headers we need but don't want installed
noinst_HEADERS = gstplugin.h
