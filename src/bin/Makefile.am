
bin_PROGRAMS = wrtctl
if BUILD_WRTCTLD
sbin_PROGRAMS = wrtctld
endif

extra_sources =
if ENABLE_STUNNEL
extra_sources += stunnel.c
endif

wrtctl_SOURCES = wrtctl.c $(extra_sources)
wrtctl_LDADD = $(top_builddir)/src/libwrtctl/libwrtctl.la -luci
wrtctl_CFLAGS = $(AM_CFLAGS) -I$(top_builddir)/src/libwrtctl/

wrtctld_SOURCES = wrtctld.c $(extra_sources)
wrtctld_LDADD = $(top_builddir)/src/libwrtctl/libwrtctl.la -luci
wrtctld_CFLAGS = $(AM_CFLAGS) -I$(top_builddir)/src/libwrtctl/

