STUNNEL_SOURCES=
if ENABLE_STUNNEL
STUNNEL_SOURCES += stunnel.c
endif

lib_LTLIBRARIES = libwrtctl.la
include_HEADERS = wrtctl-net.h wrtctl-log.h 

libwrtctl_la_SOURCES = mod.c net-client.c net-common.c net-server.c tpl.c wrtctl-log.c $(STUNNEL_SOURCES)
libwrtctl_la_LIBADD = -ldl

moddir=$(PYTHON_SITE_PKG)/wrtctl/
mod_PROGRAMS = _wrtctl.so

_wrtctl_so_SOURCES = _wrtctl_py.c
_wrtctl_so_LDADD = libwrtctl.la
_wrtctl_so_LDFLAGS = -shared $(PYTHON_LDFLAGS)
_wrtctl_so_CFLAGS = $(AM_CFLAGS) $(PYTHON_CPPFLAGS) -fPIC
