

##General Variables
CC=gcc
SHELL=/bin/bash
VBOX_HEADERS_DIR:=include/vbox
CCFLAGS=-pipe -Wall
SRC_FILES=src/vdfuse.c
OUT_DIR=bin
OUT_FILE=${OUT_DIR}/vdfuse

##OSX Variables
VBOX_OSX_BINS:= VBoxDD.dylib VBoxDDU.dylib VBoxVMM.dylib VBoxRT.dylib VBoxDD2.dylib VBoxREM.dylib
VBOX_OSX_INSTALL_DIR:=/Applications/VirtualBox.app/Contents/MacOS
FUSE_OSX_HEADERS:=/usr/local/include/osxfuse

##Linux Variabled (debian)
VBOX_LINUX_BINS:=VBoxDDU.so
VBOX_LINUX_INSTALL_DIR:=/usr/lib/virtualbox
FUSE_LINUX_HEADERS:=`pkg-config --cflags --libs fuse`

#Decide which to use based on platform
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    VBOX_BINS:=${VBOX_LINUX_BINS}
    VBOX_INSTALL_DIR:=${VBOX_LINUX_INSTALL_DIR}
    FUSE_HEADERS:=${FUSE_LINUX_HEADERS}
endif
ifeq ($(UNAME_S),Darwin)
    CCFLAGS += -arch i386 -lfuse_ino64
    VBOX_BINS:=${VBOX_OSX_BINS}
    VBOX_INSTALL_DIR:=${VBOX_OSX_INSTALL_DIR}
    FUSE_HEADERS:=${FUSE_OSX_HEADERS}
endif

VBOX_BINS:=$(addprefix $(VBOX_INSTALL_DIR)/, $(VBOX_BINS))

all: deps build

build:
	mkdir -p ${OUT_DIR}
	${CC} ${CCFLAGS} ${SRC_FILES} ${VBOX_BINS} -o ${OUT_FILE} -I${FUSE_HEADERS} -I${VBOX_HEADERS_DIR} -Wl,-rpath,${VBOX_INSTALL_DIR}

deps: vbox-headers

vbox-headers: 
	if [ -d "./$(VBOX_HEADERS_DIR)" ]; then\
		mkdir -p $(VBOX_HEADERS_DIR);\
		cd $(VBOX_HEADERS_DIR);\
		svn update;\
	else\
		svn co http://www.virtualbox.org/svn/vbox/trunk/include $(VBOX_HEADERS_DIR);\
	fi;

clean:
	rm -rf ${OUT_DIR}

#libfuse-dev:
#	if [ `pkg-config --exists fuse` -ne 0]; then\
#		echo "FUSE headers not found. Are they installed?"\
#		echo "(Run 'apt-get install libfuse-dev' on Ubuntu / Debian)"\
#	exit 1\
#	fi;
