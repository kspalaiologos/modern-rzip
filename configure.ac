
AC_PREREQ([2.69])
AC_INIT([modern-rzip], [v0.9], [kspalaiologos@gmail.com])
AC_COPYRIGHT([Copyright (C) Kamila Szewczyk, 2022. Licensed under the terms of GPLv2 license.])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([build-aux])

AC_PROG_STRIP

AC_LANG([C])
AC_PROG_CC

AX_CHECK_COMPILE_FLAG([-msse2],
  [CFLAGS="$CFLAGS -msse2"; AC_SUBST([ASOBJ], ["vendor/zstd/lib/decompress/huf_decompress_amd64.o"])],
  [CFLAGS="$CFLAGS -DNOJIT -DZSTD_DISABLE_ASM"; AC_SUBST([ASOBJ], [])], [-Werror -Wall -Wextra])
AX_CHECK_COMPILE_FLAG([-Wno-pointer-sign], [CFLAGS="$CFLAGS -Wno-pointer-sign"], , [-Werror -Wall -Wextra])
AX_CHECK_COMPILE_FLAG([-Wno-pragma-pack], [CFLAGS="$CFLAGS -Wno-pragma-pack"], , [-Werror -Wall -Wextra])

AC_CHECK_SIZEOF(long)
AS_IF([test $ac_cv_sizeof_long = 4], AC_MSG_ERROR([32-bit machines are not supported.]))

AC_CHECK_LIB(pthread, pthread_create, , AC_MSG_ERROR([libpthread missing.]))
AC_CHECK_LIB(gcrypt, gcry_md_open, , AC_MSG_ERROR([libgcrypt-dev missing.]))
AC_CHECK_LIB(m, sqrt, , AC_MSG_ERROR([libm missing.]))
AC_CHECK_LIB(gpg-error, gpg_err_code_to_errno, , AC_MSG_ERROR([libgpg-error-dev missing.]))

AC_LANG([C++])
AC_PROG_CXX

AX_CHECK_COMPILE_FLAG([-msse2], [CXXFLAGS="$CXXFLAGS -msse2"], [CXXFLAGS="$CXXFLAGS -DNOJIT -DZSTD_DISABLE_ASM"], [-Werror -Wall -Wextra])
AX_CHECK_COMPILE_FLAG([-Wno-pointer-sign], [CXXFLAGS="$CXXFLAGS -Wno-pointer-sign"], , [-Werror -Wall -Wextra])
AX_CHECK_COMPILE_FLAG([-Wno-pragma-pack], [CXXFLAGS="$CXXFLAGS -Wno-pragma-pack"], , [-Werror -Wall -Wextra])
AX_CXX_COMPILE_STDCXX([14], [ext], [])

AC_ARG_ENABLE([lto], [AS_HELP_STRING([--enable-lto], [Enable link-time optimization])], [CFLAGS="$CFLAGS -flto"; CXXFLAGS="$CXXFLAGS -flto"], [])
AC_ARG_ENABLE([static], [AS_HELP_STRING([--enable-static], [Enable static build])], [CFLAGS="$CFLAGS -static"; CXXFLAGS="$CXXFLAGS -static"], [])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
