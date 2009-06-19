
bin_PROGRAMS = wrtctl
sbin_PROGRAMS = wrtctld

wrtctl_SOURCES = wrtctl.c
wrtctl_LDADD = $(top_builddir)/src/libwrtctl/libwrtctl.la -luci
wrtctl_CFLAGS = $(AM_CFLAGS) -I$(top_builddir)/src/libwrtctl/

wrtctld_SOURCES = wrtctld.c
wrtctld_LDADD = $(top_builddir)/src/libwrtctl/libwrtctl.la -luci
wrtctld_CFLAGS = $(AM_CFLAGS) -I$(top_builddir)/src/libwrtctl/

