#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([cava_filter], [0.01], [adrian@antiprism.com])
AM_INIT_AUTOMAKE([1.9 foreign])
LT_INIT

AC_CONFIG_SRCDIR([src/utils.cpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Set compiler
AC_LANG([C++])
AX_CXX_COMPILE_STDCXX(14)
AC_LANG_PUSH([C++])

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lfftw3':
AC_CHECK_LIB([fftw3], [main])
# FIXME: Replace `main' with a function in `-lm':
AC_CHECK_LIB([m], [main])

# Checks for header files.
AC_CHECK_HEADERS([limits.h stdint.h stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset pow sqrt strchr strcspn])

AC_LANG_POP([C++])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/cavacore/Makefile
                 ])
AC_OUTPUT
