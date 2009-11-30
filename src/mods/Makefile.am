MOD_CFLAGS = -fPIC -I$(top_builddir)/src/libwrtctl/


moddir = $(libdir)/wrtctl/modules/

mod_LTLIBRARIES = sys-cmds.la uci-cmds.la

sys_cmds_la_SOURCES = sys-cmds.c
sys_cmds_la_CFLAGS = $(AM_CFLAGS) $(MOD_CFLAGS)
sys_cmds_la_LDFLAGS = -module -avoid-version -no-undefined

uci_cmds_la_SOURCES = uci-cmds.c
uci_cmds_la_CFLAGS = $(AM_CFLAGS) $(MOD_CFLAGS)
uci_cmds_la_LIBADD = -luci
uci_cmds_la_LDFLAGS = -module -avoid-version -no-undefined
