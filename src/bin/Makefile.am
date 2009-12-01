
bin_PROGRAMS = wrtctl
if BUILD_WRTCTLD
sbin_PROGRAMS = wrtctld
endif

wrtctl_SOURCES = wrtctl.c
wrtctl_LDADD = $(top_builddir)/src/libwrtctl/libwrtctl.la -luci
wrtctl_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/src/libwrtctl/

wrtctld_SOURCES = wrtctld.c
wrtctld_LDADD = $(top_builddir)/src/libwrtctl/libwrtctl.la -luci
wrtctld_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)/src/libwrtctl/

