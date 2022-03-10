AC_INIT([sairedis],[1.0])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([])
AC_CONFIG_AUX_DIR(config)
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([subdir-objects foreign serial-tests])
AC_LANG_C
AC_LANG([C++])
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_HEADER_STDC
AM_PATH_PYTHON
AM_PATH_PYTHON3
AX_CODE_COVERAGE
AX_ADD_AM_MACRO_STATIC([])

AM_CONDITIONAL(SONIC_ASIC_PLATFORM_BAREFOOT,   test x$CONFIGURED_PLATFORM = xbarefoot)

AC_ARG_ENABLE(debug,
[  --enable-debug          turn on debugging],
[case "${enableval}" in
	yes) debug=true ;;
	no)  debug=false ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
esac],[debug=false])
AM_CONDITIONAL(DEBUG, test x$debug = xtrue)

AM_COND_IF([DEBUG], AC_SUBST(DBGFLAGS, "-ggdb -DDEBUG"), AC_SUBST(DBGFLAGS, "-g"))

AM_COND_IF([DEBUG],
        [AM_COND_IF([CODE_COVERAGE_ENABLED],
            AC_MSG_ERROR("Flag conflict --enable-debug 'DEBUG' and --enable-code-coverage 'NDEBUG'"))])

AC_ARG_WITH(sai,
[  --with-sai=vs           compiling against specific SAI],
[case "${withval}" in
	vs)   sai=vs ;;
	*) AC_MSG_ERROR(bad value ${withval} for --with-sai) ;;
esac])
AM_CONDITIONAL(SAIVS,   test x$sai = xvs)

AC_ARG_WITH(swss-common-inc,
[  --with-swss-common-inc=DIR
                           prefix where swss-common includes are installed],
[AC_SUBST(CPPFLAGS, "$CPPFLAGS -I${withval}")
 AC_SUBST(SWSS_COMMON_INC_DIR, "${withval}")])

AC_ARG_WITH(swss-common-lib,
[  --with-swss-common-lib=DIR
                           prefix where swss-common libraries are installed],
[AC_SUBST(LDFLAGS, "$LDFLAGS -L${withval}")
 AC_SUBST(SWSS_COMMON_LIB_DIR, "${withval}")])

AC_ARG_ENABLE(syncd,
[  --enable-syncd          build syncd],
[case "${enableval}" in
   yes) syncd=true ;;
   no)  syncd=false ;;
   *) AC_MSG_ERROR(bad value ${enableval} for --enable-syncd) ;;
esac],[syncd=true])
AS_IF([test "x${ENABLESYNCD}" = "xno" ],[syncd=false],[])
AM_CONDITIONAL(SYNCD, test x$syncd = xtrue)
AM_COND_IF([SYNCD], [
        AM_COND_IF([SAIVS], [], [AC_CHECK_LIB([sai], [main], [AC_MSG_NOTICE(libsai found)], [AC_MSG_ERROR(libsai is required for syncd)])
])])

AC_ARG_ENABLE(rpcserver,
[  --enable-rpcserver      compile with thrift rpcserver included],
[case "${enableval}" in
   yes) rpcserver=true ;;
   no)  rpcserver=false ;;
   *) AC_MSG_ERROR(bad value ${enableval} for --enable-rpcserver) ;;
esac],[rpcserver=false])
AM_CONDITIONAL(SAITHRIFT, test x$rpcserver = xtrue)
AM_COND_IF([SAITHRIFT], [
        AC_CHECK_LIB([thrift], [main], [AC_MSG_NOTICE(libthrift found)], [AC_MSG_ERROR(libthrift is required for rpcserver)])
])

AC_ARG_ENABLE(redis-test,
[  --enable-redis-test     test with redis service],
[case "${enableval}" in
	yes) rtest=true ;;
	no)  rtest=false ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --enable-redis-test) ;;
esac],[rtest=false])
AM_CONDITIONAL(RTEST, test x$rtest = xtrue)

if test x$CONFIGURED_ARCH = xarmhf && test x$CROSS_BUILD_ENVIRON = xy; then
	AM_CONDITIONAL(ARCH64, false)
else
	AM_CONDITIONAL(ARCH64, test `getconf LONG_BIT` = "64")
fi

AC_ARG_ENABLE(asan,
[  --enable-asan      Compile with address sanitizer],
[case "${enableval}" in
	yes) asan_enabled=true ;;
	no)  asan_enabled=false ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --enable-asan) ;;
esac],[asan_enabled=false])

if test "x$asan_enabled" = "xtrue"; then
    CFLAGS_ASAN+=" -fsanitize=address"
    CFLAGS_ASAN+=" -DASAN_ENABLED"
    CFLAGS_ASAN+=" -ggdb -fno-omit-frame-pointer -U_FORTIFY_SOURCE"
    AC_SUBST(CFLAGS_ASAN)

    LDFLAGS_ASAN+=" -lasan"
    AC_SUBST(LDFLAGS_ASAN)
fi

AM_CONDITIONAL(ASAN_ENABLED, test x$asan_enabled = xtrue)

AC_PATH_PROGS(SWIG, [swig3.0 swig])

CXXFLAGS_COMMON=""
CXXFLAGS_COMMON+=" -ansi"
CXXFLAGS_COMMON+=" -fPIC"
CXXFLAGS_COMMON+=" -std=c++14"
CXXFLAGS_COMMON+=" -Wall"
CXXFLAGS_COMMON+=" -Wcast-align"
CXXFLAGS_COMMON+=" -Wcast-qual"
CXXFLAGS_COMMON+=" -Wconversion"
CXXFLAGS_COMMON+=" -Wdisabled-optimization"
CXXFLAGS_COMMON+=" -Werror"
CXXFLAGS_COMMON+=" -Wextra"
CXXFLAGS_COMMON+=" -Wfloat-equal"
CXXFLAGS_COMMON+=" -Wformat=2"
CXXFLAGS_COMMON+=" -Wformat-nonliteral"
CXXFLAGS_COMMON+=" -Wformat-security"
CXXFLAGS_COMMON+=" -Wformat-y2k"
CXXFLAGS_COMMON+=" -Wimport"
CXXFLAGS_COMMON+=" -Winit-self"
CXXFLAGS_COMMON+=" -Wno-inline"
CXXFLAGS_COMMON+=" -Winvalid-pch"
CXXFLAGS_COMMON+=" -Wmissing-field-initializers"
CXXFLAGS_COMMON+=" -Wmissing-format-attribute"
CXXFLAGS_COMMON+=" -Wmissing-include-dirs"
CXXFLAGS_COMMON+=" -Wmissing-noreturn"
CXXFLAGS_COMMON+=" -Wno-aggregate-return"
CXXFLAGS_COMMON+=" -Wno-padded"
CXXFLAGS_COMMON+=" -Wno-switch-enum"
CXXFLAGS_COMMON+=" -Wno-unused-parameter"
CXXFLAGS_COMMON+=" -Wpacked"
CXXFLAGS_COMMON+=" -Wpointer-arith"
CXXFLAGS_COMMON+=" -Wredundant-decls"
CXXFLAGS_COMMON+=" -Wshadow"
CXXFLAGS_COMMON+=" -Wstack-protector"
CXXFLAGS_COMMON+=" -Wstrict-aliasing=3"
CXXFLAGS_COMMON+=" -Wswitch"
CXXFLAGS_COMMON+=" -Wswitch-default"
CXXFLAGS_COMMON+=" -Wunreachable-code"
CXXFLAGS_COMMON+=" -Wunused"
CXXFLAGS_COMMON+=" -Wvariadic-macros"
CXXFLAGS_COMMON+=" -Wwrite-strings"
CXXFLAGS_COMMON+=" -Wno-switch-default"
CXXFLAGS_COMMON+=" -Wconversion"
CXXFLAGS_COMMON+=" -Wno-psabi"

SAVED_FLAGS="$CXXFLAGS"
CXXFLAGS="-Wcast-align=strict"
AC_MSG_CHECKING([whether CXX supports -Wcast-align=strict])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],
    [AC_MSG_RESULT([yes])]
    [AC_SUBST([CXXFLAGS_COMMON], ["${CXXFLAGS_COMMON} -Wcast-align=strict"])],
    [AC_MSG_RESULT([no])])
CXXFLAGS="$SAVED_FLAGS"

AC_SUBST(NO_CAST_FUNCTION_TYPE)

SAVED_FLAGS="$CXXFLAGS"
CXXFLAGS="-Wno-cast-function-type"
AC_MSG_CHECKING([whether CXX supports -Wno-cast-function-type])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],
    [AC_MSG_RESULT([yes])]
    [AC_SUBST([NO_CAST_FUNCTION_TYPE], ["-Wno-cast-function-type"])],
    [AC_MSG_RESULT([no])])
CXXFLAGS="$SAVED_FLAGS"

AC_SUBST(CXXFLAGS_COMMON)

AC_SUBST(SAIINC, "-I\$(top_srcdir)/SAI/inc -I\$(top_srcdir)/SAI/experimental -I\$(top_srcdir)/SAI/meta")

AC_OUTPUT(Makefile
          meta/Makefile
          lib/Makefile
          vslib/Makefile
          syncd/Makefile
          saiplayer/Makefile
          saidump/Makefile
          saisdkdump/Makefile
          saidiscovery/Makefile
          saiasiccmp/Makefile
          tests/Makefile
          unittest/Makefile
          unittest/meta/Makefile
          unittest/lib/Makefile
          unittest/vslib/Makefile
          unittest/syncd/Makefile
          pyext/Makefile
          pyext/py2/Makefile
          pyext/py3/Makefile)
