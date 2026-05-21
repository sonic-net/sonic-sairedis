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
AM_PATH_PYTHON3
AX_PYTHON3_DEVEL
AX_CODE_COVERAGE
AX_ADD_AM_MACRO_STATIC([])

AM_CONDITIONAL(SONIC_ASIC_PLATFORM_BAREFOOT,   test x$CONFIGURED_PLATFORM = xbarefoot)
AM_CONDITIONAL(SONIC_ASIC_PLATFORM_BROADCOM,   test x$CONFIGURED_PLATFORM = xbroadcom)
AM_CONDITIONAL(SONIC_ASIC_PLATFORM_MELLANOX,   test x$CONFIGURED_PLATFORM = xmellanox)
AM_CONDITIONAL(SONIC_ASIC_PLATFORM_CLOUNIX,   test x$CONFIGURED_PLATFORM = xclounix)

AM_COND_IF([SONIC_ASIC_PLATFORM_MELLANOX],
           AC_DEFINE([MELLANOX], [1], [Define to 1 on Mellanox Platform]))

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

AC_ARG_WITH(dashsai,
    [ --with-dashsai         compiling against DASH SAI],
    [AM_CONDITIONAL(DASHSAI,   true)],
    [AM_CONDITIONAL(DASHSAI,   false)])

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

AC_ARG_ENABLE(python2,
[  --enable-python2     Generate Python 2 bindings],
[case "${enableval}" in
	yes) python2=true ;;
	no)  python2=false ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --enable-python2) ;;
esac],[python2=true])

if test "x$asan_enabled" = "xtrue"; then
    CFLAGS_ASAN+=" -fsanitize=address"
    CFLAGS_ASAN+=" -DASAN_ENABLED"
    CFLAGS_ASAN+=" -ggdb -fno-omit-frame-pointer -U_FORTIFY_SOURCE"
    CFLAGS_ASAN+=" -Wno-maybe-uninitialized"
    AC_SUBST(CFLAGS_ASAN)

    LDFLAGS_ASAN+=" -lasan"
    AC_SUBST(LDFLAGS_ASAN)
fi

if test "x$python2" = "xtrue"; then
	m4_define_default([_AM_PYTHON_INTERPRETER_LIST],
					  [python2 python2.7 python2.6 python2.5 python2.4 python2.3 python2.2 python2.1 python2.0])
	AM_PATH_PYTHON
	AX_PYTHON_DEVEL([< '3'])
	PYTHON2_CPPFLAGS=$PYTHON_CPPFLAGS
	PYTHON2_LIBS=$PYTHON_LIBS
	AC_SUBST(PYTHON2_CPPFLAGS)
	AC_SUBST(PYTHON2_LIBS)
else
	# The py-compile script either uses $PYTHON (which won't be set if we're not building python 2 bindings),
	# or assumes a default of `python` (which may or may not exist as a symlink). Therefore, just assign
	# PYTHON3 to PYTHON to make sure it always points to a valid binary.
	PYTHON=$PYTHON3
fi

AM_CONDITIONAL(ASAN_ENABLED, test x$asan_enabled = xtrue)
AM_CONDITIONAL(PYTHON2, test x$python2 = xtrue)

AC_PATH_PROGS(SWIG, [swig4.0 swig3.0 swig])

CXXFLAGS_COMMON=""
CXXFLAGS_COMMON+=" -ansi"
CXXFLAGS_COMMON+=" -fPIC"
CXXFLAGS_COMMON+=" -pipe"
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

# -lvlibapi -lvapiclient -lvppapiclient -lvlibmemoryclient -lsvm -lvppinfra -lvlib -lvatplugin
# -lvapiclient -lsvm -lvatplugin
AC_CHECK_LIB([vlib], [main], [AC_SUBST(VPP_LIBS, "-lvlib -lvlibapi -lvppapiclient -lvlibmemoryclient -lvppinfra")])
AM_CONDITIONAL([USE_VPP], [test "x$VPP_LIBS" != "x"])

AC_ARG_WITH(extra-libsai-ldflags,
[  --with-extra-libsai-ldflags=FLAGS
                          extra libsai.so flags for vendor library],
[AC_SUBST(EXTRA_LIBSAI_LDFLAGS, "${withval}")])

AC_SUBST(SAIINC, "-I\$(top_srcdir)/SAI/inc -I\$(top_srcdir)/SAI/experimental -I\$(top_srcdir)/SAI/meta")

AM_COND_IF([SYNCD], [
AM_COND_IF([SAIVS], [], [
SAVED_FLAGS="$CXXFLAGS"
CXXFLAGS="-Xlinker --no-as-needed -lsai -I$srcdir/SAI/inc -I$srcdir/SAI/experimental -I$srcdir/SAI/meta $EXTRA_LIBSAI_LDFLAGS"
AC_CHECK_FUNCS(sai_query_api_version, [
AC_MSG_CHECKING([SAI headers API version and library version check])
AC_TRY_RUN([
extern "C" {
#include <sai.h>
#include <cstdio>
}
int main() {
    sai_api_version_t version;
    if (SAI_STATUS_SUCCESS != sai_query_api_version(&version))
    {
        return 1;
    }
/*
since commit 8e1fb37 (v1.8.0) there is check enum binary backward compatibility
so we are relaxing this check below, versions do not have to be equal but both
need to be at least 2ebde24 (v1.9.0), this will make sure that enums are always
ok, but since that commit some data structures changed and are not binary
backwad compatible like next hop group api, acl_capability, structs are
backward binary compatible from commit aed34c8, which closest tag version for
it is commit 3ff228a (v1.12.0), so min version check should be set to
(v1.12.0), but some production branches do not use that high SAI version yet,
some use even version v1.7.0 which then it is impossible to provide even enum
backward compatibility, check those links:
https://github.com/opencomputeproject/SAI/pull/1297 (enums)
https://github.com/opencomputeproject/SAI/pull/1795 (structs)
*/
    sai_api_version_t minversion = SAI_VERSION(1,9,0);
    printf("min SAI version: %ld, sairedis SAI headers version: %ld, vendor lib SAI query api version: %ld\n",
           minversion, SAI_API_VERSION, version);
    return (version < minversion) || (SAI_API_VERSION < minversion);
}],
[AC_MSG_RESULT(ok)],
[AC_MSG_RESULT(failed)
AC_MSG_ERROR("SAI headers API version and library version mismatch")])],
[AC_MSG_ERROR("SAI library libsai.so does not have sai_query_api_version API which is required")])
CXXFLAGS="$SAVED_FLAGS"
])])

AM_COND_IF([SAIVS],
[
    AC_DEFINE([HAVE_SAI_BULK_OBJECT_CLEAR_STATS], [1], [])
    AC_DEFINE([HAVE_SAI_BULK_OBJECT_GET_STATS], [1], [])
    AC_DEFINE([HAVE_SAI_QUERY_STATS_ST_CAPABILITY], [1], [])
    AC_DEFINE([HAVE_SAI_TAM_TELEMETRY_GET_DATA], [1], [])
],
[
    AM_COND_IF([SYNCD], [
    SAVED_FLAGS="$CXXFLAGS"
    CXXFLAGS="-Xlinker --no-as-needed -lsai -I$srcdir/SAI/inc -I$srcdir/SAI/experimental -I$srcdir/SAI/meta"
    AC_CHECK_FUNCS(sai_bulk_object_clear_stats sai_bulk_object_get_stats sai_query_stats_st_capability sai_tam_telemetry_get_data)
    CXXFLAGS="$SAVED_FLAGS"
    ])
])

AC_DEFINE([SAIREDIS_GIT_REVISION],
        "[m4_esyscmd([echo -n $(git rev-parse --short HEAD || echo 0000000)])]",
        [sairedis git revision information])

AC_DEFINE([SAI_GIT_REVISION],
        "[m4_esyscmd([echo -n $(cd SAI && git rev-parse --short HEAD || echo 0000000)])]",
        [SAI git revision information])

AC_OUTPUT(Makefile
          meta/Makefile
          lib/Makefile
          vslib/Makefile
          syncd/Makefile
          syncd/tests/Makefile
          saiplayer/Makefile
          saidump/Makefile
          saisdkdump/Makefile
          saidiscovery/Makefile
          saiasiccmp/Makefile
          tests/Makefile
          proxylib/Makefile
          unittest/Makefile
          unittest/meta/Makefile
          unittest/lib/Makefile
          unittest/vslib/Makefile
          unittest/syncd/Makefile
          unittest/proxylib/Makefile
          unittest/saidump/Makefile
          pyext/Makefile
          pyext/py2/Makefile
          pyext/py3/Makefile)
