STUNNEL_SOURCES=
if ENABLE_STUNNEL
STUNNEL_SOURCES += stunnel.c
endif

lib_LTLIBRARIES = libwrtctl.la
include_HEADERS = wrtctl-net.h wrtctl-log.h 

libwrtctl_la_SOURCES = mod.c net-client.c net-common.c net-server.c tpl.c wrtctl-log.c $(STUNNEL_SOURCES)
libwrtctl_la_LIBADD = -ldl



