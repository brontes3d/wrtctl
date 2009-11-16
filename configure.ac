AC_PREREQ([2.63])
AC_INIT([wrtctl], [0.1], [http://www.brontes3d.com/opensource])
AC_CONFIG_AUX_DIR(build-aux)

AM_INIT_AUTOMAKE([dist-bzip2 -Wno-portability])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

AC_GNU_SOURCE

AC_PROG_CC
AC_PROG_CPP
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_CC_C_O


AC_CHECK_LIB([dl], [dlopen])
AC_CHECK_LIB([uci], [uci_alloc_context])

AC_CHECK_HEADERS([ \
    fcntl.h \
    inttypes.h \
    netdb.h \
    netinet/in.h \
    stddef.h \
    stdlib.h \
    string.h \
    sys/param.h \
    sys/socket.h \
    unistd.h])

AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_CHECK_TYPES([ptrdiff_t])

AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([ftruncate memset munmap select setenv socket strchr strdup strerror strnlen strspn])


AC_ARG_WITH( moduledir,
    [  --with-moduledir=DIR    Base directory for dynamically loadable modules],
    moduledir="$withval",
    moduledir="${libdir}/wrtctl")

AC_ARG_ENABLE( python,
    [  --enable-python             Build the python client module [[default=yes]] ],
    enable_python="$enableval",
    enable_python=yes)

if test x"$enable_python" = xyes; then
    AM_PATH_PYTHON

    AC_PYTHON_DEVEL( >= '2.5' )
    AC_SUBST(PYTHON_LDFLAGS)
    AC_SUBST(PYTHON_CPPFLAGS)
    AC_SUBST(PYTHON_SITE_PKG)
fi
AM_CONDITIONAL(ENABLE_PYTHON, test x"$enable_python" = xyes)

AC_ARG_ENABLE( wrtctld,
    [  --enable-wrtctld            Build the wrtctl daemon [[default=yes]] ],
    build_wrtctld="$enableval",
    build_wrtctld=yes)
AM_CONDITIONAL(BUILD_WRTCTLD, test x"$build_wrtctld" = xyes)

AC_ARG_ENABLE( internal-queue-h,
    [  --enable-internal-queue-h   Use internal copy of queue.h (for older systems) [[default=no]] ],
    AC_DEFINE_UNQUOTED( [INTERNAL_QUEUE_H], [1], ["Use internal copy of queue.h"])
)

AC_ARG_ENABLE( stunnel,
    [  --enable-stunnel[=PATH]   Enable stunnel wrappers [[no, /usr/bin/stunnel]]],
    stunnel_path="$withval",
    stunnel_path="no")

if test x"$stunnel_path" != xno; then
    AM_CONDITIONAL(ENABLE_STUNNEL, true)
    AC_DEFINE_UNQUOTED( [ENABLE_STUNNEL],
        [],
        "Enable support for stunnel wrappers.")
    if test x"$stunnel_path" = xyes; then
        AC_DEFINE_UNQUOTED( [STUNNEL_PATH],
            "/usr/bin/stunnel",
            "Path to stunnel executable." )
    else
        AC_DEFINE_UNQUOTED( [STUNNEL_PATH],
            "$stunnel_path",
            "Path to stunnel executable." )
    fi
else
    AM_CONDITIONAL(ENABLE_STUNNEL, false)
fi
 
tmp_moduledir="`expr "$moduledir" : '${exec_prefix}\(.*\)'`"
if test -n "$tmp_moduledir"; then
    if test x"$prefix" = xNONE; then
        AC_DEFINE_UNQUOTED( [DEFAULT_MODULE_DIR],
            ["${ac_default_prefix}${tmp_moduledir}"],
            "Default location to look for modules.")
    else
        AC_DEFINE_UNQUOTED( [DEFAULT_MODULE_DIR],
            ["${prefix}${tmp_moduledir}"],
            "Default location to look for modules.")
    fi 
else
    AC_DEFINE_UNQUOTED( [DEFAULT_MODULE_DIR],
        ["${moduledir}"],
        "Default location to look for modules.")
fi

keydir="`expr "${sysconfdir}"/wrtctl/key.pem : '${prefix}\(.*\)'`" 
AC_DEFINE_UNQUOTED( [DEFAULT_KEY_PATH],
    ["$keydir"],
    "Default key path when using stunnel wrappers")

AC_CONFIG_FILES([
    Makefile
    src/Makefile
    src/bin/Makefile
    src/libwrtctl/Makefile
    src/mods/Makefile
    test/Makefile
    util/Makefile
])

AC_OUTPUT

