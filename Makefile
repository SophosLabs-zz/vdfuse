

CC=gcc
SHELL=/bin/bash
VBOX_HEADERS_DIR=include/
VBOX_INSTALL_DIR=/usr/lib/virtualbox
LIBFUSE_OPTS=`pkg-config --cflags --libs fuse`

CFLAGS=-pipe
SRC_FILES=vdfuse.c
OUT_FILE=vdfuse


all: deps build

build:
	#${CC} ${SRC_FILES} -o ${OUT_FILE} ${LIBFUSE_OPTS} -I${VBOX_HEADERS_DIR} -Wl,-rpath,"${VBOX_INSTALL_DIR}" -l:${VBOX_INSTALL_DIR}/VBoxDDU.so -Wall ${CFLAGS}
	${CC} ${SRC_FILES} -o ${OUT_FILE} ${LIBFUSE_OPTS} -I${VBOX_HEADERS_DIR} -l:${VBOX_INSTALL_DIR}/VBoxDDU.so -Wall ${CFLAGS}

deps: vbox-headers

vbox-headers: 
	if [ -d "./$(VBOX_HEADERS_DIR)" ]; then\
		mkdir -p $(VBOX_HEADERS_DIR);\
		cd $(VBOX_HEADERS_DIR);\
		svn update;\
	else\
		svn co http://www.virtualbox.org/svn/vbox/trunk/include $(VBOX_HEADERS_DIR);\
	fi;

#libfuse-dev:
#	if [ `pkg-config --exists fuse` -ne 0]; then\
#		echo "FUSE headers not found. Are they installed?"\
#		echo "(Run 'apt-get install libfuse-dev' on Ubuntu / Debian)"\
#	exit 1\
#	fi;
