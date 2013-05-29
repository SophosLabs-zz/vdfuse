

bin_PROGRAMS=vdfuse
AM_CPPFLAGS = -Iinclude -I@FUSE_HEADERS@ @ARCH_FLAG@

vdfuse_SOURCES=src/vdfuse.c
vdfuse_LDADD=$(addprefix @VBOX_INSTALL_DIR@/, @VBOX_BINS@)
vdfuse_LDFLAGS=-Wl,-rpath,@VBOX_INSTALL_DIR@ @ARCH_FLAG@
#
#EXTRA_DIST = autogen.sh
