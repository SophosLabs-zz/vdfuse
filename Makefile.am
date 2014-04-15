

bin_PROGRAMS=vdfuse
AM_CFLAGS = -Iinclude @FUSE_HEADERS@
vdfuse_SOURCES=src/vdfuse.c
vdfuse_LDADD=$(addprefix @VBOX_INSTALL_DIR@/, @VBOX_BINS@) @FUSE_FLAG@
vdfuse_LDFLAGS=-Wl,-rpath,@VBOX_INSTALL_DIR@

EXTRA_DIST = autogen.sh
