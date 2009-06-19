MOD_CFLAGS = -shared -rdynamic -fPIC -I$(top_builddir)/src/libwrtctl/


moddir = $(libdir)/wrtctl/modules/

mod_PROGRAMS = sys-cmds.so uci-cmds.so

sys_cmds_so_SOURCES = sys-cmds.c
sys_cmds_so_CFLAGS = $(AM_CFLAGS) $(MOD_CFLAGS)

uci_cmds_so_SOURCES = uci-cmds.c
uci_cmds_so_CFLAGS = $(AM_CFLAGS) $(MOD_CFLAGS)
uci_cmds_so_LDADD = -luci

