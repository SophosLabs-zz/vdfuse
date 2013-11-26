#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.67])
AC_INIT([vdfuse], [0.9], [chris.benninger@sophos.com])
AC_CONFIG_SRCDIR([src/vdfuse.c])
AC_CONFIG_HEADERS([src/config.h:config.in])

AM_INIT_AUTOMAKE
#AM_INIT_AUTOMAKE(vdfuse, 0.9)

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_LANG([C])
CFLAGS=" -D_FILE_OFFSET_BITS=64"
# Checks for libraries.

AC_CHECK_PROG(SVN,svn,[yes],[AC_MSG_ERROR(Could not find svn)])

AC_CANONICAL_HOST
case "$host_os" in
  darwin* )
        FUSE_FLAG="-losxfuse"
        ARCH=$(uname -p)
        CFLAGS+=" -D_DARWIN_USE_64_BIT_INODE -arch ${ARCH} ${FUSE_FLAG}"
        VBOX_INSTALL_DIR="/Applications/VirtualBox.app/Contents/MacOS"
        VBOX_BINS="VBoxDD.dylib VBoxDDU.dylib VBoxVMM.dylib VBoxRT.dylib VBoxDD2.dylib VBoxREM64.dylib"

        #fuse headers override
        AC_ARG_WITH([fuse-headers],[AS_HELP_STRING([--with-fuse-headers],[location of the fuse headers])],
            [FUSE_HEADERS_DIR="$withval"],
            [FUSE_HEADERS_DIR="/usr/local/include/osxfuse"],
            )

        AC_CHECK_FILE(${FUSE_HEADERS_DIR}/fuse.h,,[AC_MSG_ERROR([Could not find fuse headers, Please install osxfuse development package])])
        FUSE_HEADERS=-I${FUSE_HEADERS_DIR}
        AC_CHECK_LIB([osxfuse], [main],[break],[AC_MSG_ERROR(Could not find fuse)])
    
        ;;
  linux-gnu*)
        FUSE_FLAG=$(pkg-config --libs fuse)
        VBOX_INSTALL_DIR="/usr/lib/virtualbox"
        VBOX_BINS="VBoxDDU.so"

        #fuse headers override
        AC_ARG_WITH([fuse-headers],[AS_HELP_STRING([--with-fuse-headers],[location of the fuse headers])],
            [
                FUSE_HEADERS_DIR="$withval"
                AC_CHECK_FILE(${FUSE_HEADERS_DIR}/fuse.h,,[AC_MSG_ERROR([Could not find fuse headers])])
                FUSE_HEADERS=-I${FUSE_HEADERS_DIR}
            ],
            [
                FUSE_HEADERS=$(pkg-config --cflags-only-I fuse)
                AC_CHECK_HEADER(fuse.h,,[AC_MSG_ERROR([Could not find fuse headers])])
            ]
            )

        # Checks for Fuse header files.
        AC_CHECK_LIB([fuse], [main],[break],[AC_MSG_ERROR(Could not find fuse)])
        ;;
  *)
        #Default Case
        AC_MSG_ERROR([Your platform is not currently supported])
        ;;
esac

AC_SUBST(FUSE_HEADERS)
AC_SUBST(FUSE_FLAG)

# Checks for VBox header files.
AC_CHECK_FILE([include/VBox/vd.h],,[AC_MSG_ERROR([Could not find vbox headers, please run 'fetch_vbox_headers.sh'])])

AC_SUBST(VBOX_INSTALL_DIR)
AC_SUBST(VBOX_BINS)
AC_SUBST(VBOX_BINS_FULL)

#AC_PATH_X
#AC_FUNC_ALLOCA

# Checks for typedefs, structures, and compiler characteristics.
#AC_CHECK_HEADER_STDBOOL
#AC_TYPE_UID_T
#AC_C_INLINE
#AC_TYPE_INT16_T
#AC_TYPE_INT32_T
#AC_TYPE_INT64_T
#AC_TYPE_INT8_T
#AC_TYPE_MODE_T
#AC_TYPE_OFF_T
#AC_TYPE_SIZE_T
#AC_TYPE_SSIZE_T
#AC_STRUCT_ST_BLOCKS
#AC_TYPE_UINT16_T
#AC_TYPE_UINT32_T
#AC_TYPE_UINT64_T
#AC_TYPE_UINT8_T
#AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
#AC_FUNC_REALLOC
#AC_CHECK_FUNCS([fesetround memchr memmove memset strpbrk])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
