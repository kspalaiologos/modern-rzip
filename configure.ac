
AC_PREREQ([2.69])
AC_INIT([modern-rzip], [v0.9], [kspalaiologos@gmail.com])
AC_COPYRIGHT([Copyright (C) Kamila Szewczyk, 2020. Licensed under the terms of MIT license.])

AC_DEFUN([AX_CHECK_COMPILE_FLAG],
[AC_PREREQ(2.64)dnl for _AC_LANG_PREFIX and AS_VAR_IF
AS_VAR_PUSHDEF([CACHEVAR],[ax_cv_check_[]_AC_LANG_ABBREV[]flags_$4_$1])dnl
AC_CACHE_CHECK([whether _AC_LANG compiler accepts $1], CACHEVAR, [
  ax_check_save_flags=$[]_AC_LANG_PREFIX[]FLAGS
  _AC_LANG_PREFIX[]FLAGS="$4 $1"
  AC_COMPILE_IFELSE([m4_default([$5],[AC_LANG_PROGRAM()])],
    [AS_VAR_SET(CACHEVAR,[yes])],
    [AS_VAR_SET(CACHEVAR,[no])])
  _AC_LANG_PREFIX[]FLAGS=$ax_check_save_flags])
AS_VAR_IF(CACHEVAR,yes,
  [m4_default([$2], :)],
  [m4_default([$3], :)])
AS_VAR_POPDEF([CACHEVAR])dnl
])dnl AX_CHECK_COMPILE_FLAGS

AC_DEFUN([AC_PROG_STRIP], [AC_CHECK_TOOL(STRIP, strip, :)])
AC_PROG_STRIP

AC_LANG([C])
AC_PROG_CC([clang gcc icc])

AX_CHECK_COMPILE_FLAG([-static], [], [AC_MSG_ERROR([C compiler does not support -static.])])
AX_CHECK_COMPILE_FLAG([-flto], [CFLAGS="$CFLAGS -flto"])
AX_CHECK_COMPILE_FLAG([-msse2], [CFLAGS="$CFLAGS -msse2"], [CFLAGS="$CFLAGS -DNOJIT -DZSTD_DISABLE_ASM"])
AX_CHECK_COMPILE_FLAG([-Wno-pointer-sign], [CFLAGS="$CFLAGS -Wno-pointer-sign"])
AX_CHECK_COMPILE_FLAG([-Wno-pragma-pack], [CFLAGS="$CFLAGS -Wno-pragma-pack"])

AC_CHECK_SIZEOF(long)
AS_IF([test $ac_cv_sizeof_long = 4], AC_MSG_ERROR([32-bit machines are not supported.]))

AC_CHECK_LIB(pthread, pthread_create, , AC_MSG_ERROR([libpthread missing.]))
AC_CHECK_LIB(gcrypt, gcry_md_open, , AC_MSG_ERROR([libgcrypt missing.]))
AC_CHECK_LIB(m, sqrt, , AC_MSG_ERROR([libm missing.]))
AC_CHECK_LIB(gpg-error, gpg_err_code_to_errno, , AC_MSG_ERROR([libgpg-error-dev missing.]))

AC_LANG([C++])
AC_PROG_CXX([clang++ g++ icpc])

AX_CHECK_COMPILE_FLAG([-static], [], [AC_MSG_ERROR([C++ compiler does not support -static.])])
AX_CHECK_COMPILE_FLAG([-flto], [CXXFLAGS="$CXXFLAGS -flto"])
AX_CHECK_COMPILE_FLAG([-msse2], [CXXFLAGS="$CXXFLAGS -msse2"], [CXXFLAGS="$CXXFLAGS -DNOJIT -DZSTD_DISABLE_ASM"])
AX_CHECK_COMPILE_FLAG([-Wno-pointer-sign], [CXXFLAGS="$CXXFLAGS -Wno-pointer-sign"])
AX_CHECK_COMPILE_FLAG([-Wno-pragma-pack], [CXXFLAGS="$CXXFLAGS -Wno-pragma-pack"])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
