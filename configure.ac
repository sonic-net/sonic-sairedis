AC_INIT([sairedis],[1.0])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([])
AC_CONFIG_AUX_DIR(config)
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([subdir-objects])
AC_LANG_C
AC_LANG([C++])
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_HEADER_STDC

AC_ARG_ENABLE(debug,
[  --enable-debug    Turn on debugging],
[case "${enableval}" in
	yes) debug=true ;;
	no)  debug=false ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
esac],[debug=false])
AM_CONDITIONAL(DEBUG, test x$debug = xtrue)

AC_ARG_WITH(sai,
[  --with-sai        Compiling against specific SAI],
[case "${withval}" in
	bcm)  sai=bcm ;;
	mlnx) sai=mlnx ;;
	*) AC_MSG_ERROR(bad value ${withval} for --with-sai) ;;
esac])
AM_CONDITIONAL(SAIBCM,  test x$sai = xbcm)
AM_CONDITIONAL(SAIMLNX, test x$sai = xmlnx)

CFLAGS_COMMON="-std=c++11 -Wall -fPIC -Wno-write-strings"
AC_SUBST(CFLAGS_COMMON)

AC_OUTPUT(Makefile lib/Makefile lib/src/Makefile syncd/Makefile)
