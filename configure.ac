AC_INIT([sairedis],[1.0])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([])
AC_CONFIG_AUX_DIR(config)
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([subdir-objects foreign])
AC_LANG_C
AC_LANG([C++])
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_HEADER_STDC

AM_CONDITIONAL(sonic_asic_platform_barefoot,   test x$CONFIGURED_PLATFORM = xbarefoot)
AM_CONDITIONAL(sonic_asic_platform_mellanox,   test x$CONFIGURED_PLATFORM = xmellanox)

AC_ARG_ENABLE(debug,
[  --enable-debug          turn on debugging],
[case "${enableval}" in
	yes) debug=true ;;
	no)  debug=false ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
esac],[debug=false])
AM_CONDITIONAL(DEBUG, test x$debug = xtrue)

AC_ARG_WITH(sai,
[  --with-sai              compiling against specific SAI],
[case "${withval}" in
	vs)   sai=vs ;;
	*) AC_MSG_ERROR(bad value ${withval} for --with-sai) ;;
esac])
AM_CONDITIONAL(SAIVS,   test x$sai = xvs)

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

CFLAGS_COMMON=""
CFLAGS_COMMON+=" -ansi"
CFLAGS_COMMON+=" -fPIC"
CFLAGS_COMMON+=" -std=c++11"
CFLAGS_COMMON+=" -Wall"
CFLAGS_COMMON+=" -Wcast-align"
CFLAGS_COMMON+=" -Wcast-qual"
CFLAGS_COMMON+=" -Wconversion"
CFLAGS_COMMON+=" -Wdisabled-optimization"
CFLAGS_COMMON+=" -Werror"
CFLAGS_COMMON+=" -Wextra"
CFLAGS_COMMON+=" -Wfloat-equal"
CFLAGS_COMMON+=" -Wformat=2"
CFLAGS_COMMON+=" -Wformat-nonliteral"
CFLAGS_COMMON+=" -Wformat-security"
CFLAGS_COMMON+=" -Wformat-y2k"
CFLAGS_COMMON+=" -Wimport"
CFLAGS_COMMON+=" -Winit-self"
CFLAGS_COMMON+=" -Winline"
CFLAGS_COMMON+=" -Winvalid-pch"
CFLAGS_COMMON+=" -Wlong-long"
CFLAGS_COMMON+=" -Wmissing-field-initializers"
CFLAGS_COMMON+=" -Wmissing-format-attribute"
CFLAGS_COMMON+=" -Wmissing-include-dirs"
CFLAGS_COMMON+=" -Wmissing-noreturn"
CFLAGS_COMMON+=" -Wno-aggregate-return"
CFLAGS_COMMON+=" -Wno-padded"
CFLAGS_COMMON+=" -Wno-switch-enum"
CFLAGS_COMMON+=" -Wno-unused-parameter"
CFLAGS_COMMON+=" -Wpacked"
CFLAGS_COMMON+=" -Wpointer-arith"
CFLAGS_COMMON+=" -Wredundant-decls"
CFLAGS_COMMON+=" -Wshadow"
CFLAGS_COMMON+=" -Wstack-protector"
CFLAGS_COMMON+=" -Wstrict-aliasing=3"
CFLAGS_COMMON+=" -Wswitch"
CFLAGS_COMMON+=" -Wswitch-default"
CFLAGS_COMMON+=" -Wunreachable-code"
CFLAGS_COMMON+=" -Wunused"
CFLAGS_COMMON+=" -Wvariadic-macros"
CFLAGS_COMMON+=" -Wwrite-strings"
CFLAGS_COMMON+=" -Wno-switch-default"
CFLAGS_COMMON+=" -Wconversion"
CFLAGS_COMMON+=" -Wlong-long"

AC_SUBST(CFLAGS_COMMON)

AC_OUTPUT(Makefile
          meta/Makefile
          lib/Makefile
          lib/src/Makefile
          vslib/Makefile
          vslib/src/Makefile
          syncd/Makefile
          saiplayer/Makefile
          saidump/Makefile
          saisdkdump/Makefile
          saidiscovery/Makefile
          tests/Makefile)
