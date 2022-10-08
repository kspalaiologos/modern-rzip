
AC_PREREQ([2.69])
AC_INIT([modern-rzip], [v0.9], [kspalaiologos@gmail.com])
AC_COPYRIGHT([Copyright (C) Kamila Szewczyk, 2022. Licensed under the terms of GPLv2 license.])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([build-aux])

AC_PROG_STRIP

AC_LANG([C])
AC_PROG_CC

MRZIP_MAJOR=`git describe --tags --abbrev=0 | sed 's/v//' | cut -d. -f1`
MRZIP_MINOR=`git describe --tags --abbrev=0 | sed 's/v//' | cut -d. -f2`
MRZIP_PATCH=`git describe --tags --abbrev=0 | sed 's/v//' | cut -d. -f3`
VERSION="-DMRZIP_MAJOR=$MRZIP_MAJOR -DMRZIP_MINOR=$MRZIP_MINOR -DMRZIP_PATCH=$MRZIP_PATCH"
AC_SUBST([VERSION])

AC_CHECK_HEADER([windows.h], [AC_SUBST([WINDOWS], [1])], [AC_SUBST([WINDOWS], [0])])

AX_CHECK_COMPILE_FLAG([-msse2],
  [CFLAGS="$CFLAGS -msse2"; AC_SUBST([ASOBJ], ["vendor/zstd/lib/decompress/huf_decompress_amd64.o"])],
  [CFLAGS="$CFLAGS -DNOJIT -DZSTD_DISABLE_ASM"; AC_SUBST([ASOBJ], [])], [-Werror -Wall -Wextra])

AC_CHECK_SIZEOF(long long)
AS_IF([test $ac_cv_sizeof_long_long = 4], AC_MSG_ERROR([32-bit machines are not supported.]))

AC_CHECK_LIB(pthread, pthread_create, , AC_MSG_ERROR([libpthread missing.]))
AC_CHECK_LIB(gcrypt, gcry_md_open, , AC_MSG_ERROR([libgcrypt-dev missing.]))
AC_CHECK_LIB(m, sqrt, , AC_MSG_ERROR([libm missing.]))
AC_CHECK_LIB(gpg-error, gpg_err_code_to_errno, , AC_MSG_ERROR([libgpg-error-dev missing.]))
AC_CHECK_LIB(intl, libintl_gettext, [AC_SUBST([LIBINTL], [-lintl])], [AC_SUBST([LIBINTL], [])])
AC_CHECK_LIB(iconv, libiconv, [AC_SUBST([LIBICONV], [-liconv])], [AC_SUBST([LIBICONV], [])])

AC_CHECK_FUNC([ffsll], [], [CFLAGS="$CFLAGS -DNO_FFSLL"])

AC_LANG([C++])
AC_PROG_CXX

AX_CHECK_COMPILE_FLAG([-msse2], [CXXFLAGS="$CXXFLAGS -msse2"], [CXXFLAGS="$CXXFLAGS -DNOJIT -DZSTD_DISABLE_ASM"], [-Werror -Wall -Wextra])
AX_CXX_COMPILE_STDCXX([11], [ext], [])

AC_ARG_ENABLE([lto], [AS_HELP_STRING([--enable-lto], [Enable link-time optimization])], [CFLAGS="$CFLAGS -flto"; CXXFLAGS="$CXXFLAGS -flto"], [])
AC_ARG_ENABLE([static], [AS_HELP_STRING([--enable-static], [Enable static build])], [CFLAGS="$CFLAGS -static"; CXXFLAGS="$CXXFLAGS -static"], [])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

GITVER=`git describe --tags --abbrev=0`

echo
echo
echo "------------------------------------------------------------------------"
echo "MODERN-RZIP $MRZIP_MAJOR.$MRZIP_MINOR.$MRZIP_PATCH ($GITVER) CONFIGURATION SUMMARY"
echo "------------------------------------------------------------------------"
echo 
echo "Compilation............: make (or gmake)"
echo "  CFLAGS...............: $CFLAGS"
echo "  CXXFLAGS.............: $CXXFLAGS"
echo "  CC...................: $CC"
echo "  CXX..................: $CXX"
echo "  STRIP................: $STRIP"
echo "  WINDOWS..............: $WINDOWS"
echo "  VERSION..............: $VERSION"
