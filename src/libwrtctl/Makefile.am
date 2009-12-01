STUNNEL_SOURCES=
if ENABLE_STUNNEL
STUNNEL_SOURCES += stunnel.c
endif

lib_LTLIBRARIES	= libwrtctl.la
include_HEADERS = wrtctl-net.h wrtctl-log.h 

EXTRA_DIST = wrtctl-int.h tpl.h queue.h

libwrtctl_la_SOURCES 	=  $(STUNNEL_SOURCES) \
	mod.c \
	net-client.c \
	net-common.c \
	net-server.c \
	tpl.c \
	wrtctl-log.c
libwrtctl_la_LIBADD 	= -ldl

if ENABLE_PYTHON
moddir			= $(prefix)$(PYTHON_SITE_PKG)/wrtctl/
mod_LTLIBRARIES = _wrtctl.la
dist_mod_DATA 		= wrtctl.py

_wrtctl_la_SOURCES	= _wrtctl_py.c
_wrtctl_la_LIBADD 	= libwrtctl.la
_wrtctl_la_LDFLAGS 	= $(PYTHON_LDFLAGS) -module -no-version
_wrtctl_la_CFLAGS 	= $(AM_CFLAGS) $(PYTHON_CPPFLAGS) -fPIC
endif
