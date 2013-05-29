#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([vdfuse], [0.9], [chris.benninger@sophos.com])
AC_CONFIG_SRCDIR([src/vdfuse.c])
AC_CONFIG_HEADERS([src/config.h:config.in])

AM_INIT_AUTOMAKE

AC_LANG([C])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lfuse_ino64':
AC_CHECK_LIB([fuse_ino64], [main])

AC_CHECK_PROG(SVN,svn,[yes],[AC_MSG_ERROR(Could not find svn)])


# Checks for header files.
#AC_CHECK_HEADERS([include/VBox/vd.h],,[AC_MSG_ERROR([Could not find vbox headers, please run 'fetch_vbox_headers.sh'])])
AC_CHECK_FILE([include/VBox/vd.h],,[AC_MSG_ERROR([Could not find vbox headers, please run 'fetch_vbox_headers.sh'])])
#AC_CHECK_HEADERS(include/VBox/vd.h,,[$( svn co http://www.virtualbox.org/svn/vbox/trunk/include include/vbox )])


AC_PATH_X
AC_FUNC_ALLOCA

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_STRUCT_ST_BLOCKS
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_REALLOC
AC_CHECK_FUNCS([fesetround memchr memmove memset strpbrk])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
