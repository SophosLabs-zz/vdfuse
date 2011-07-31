#!/bin/sh

# http://serverfault.com/questions/174278/mount-an-vhd-on-mac-os-x
# Authors: Chealion and janm


infile=vdfuse.c
outfile=vdfuse
incdir="include/"
INSTALL_DIR="/Applications/VirtualBox.app/Contents/MacOS"
CFLAGS="-pipe"

gcc -arch i386 "${infile}" \
         "${INSTALL_DIR}"/VBoxDD.dylib \
         "${INSTALL_DIR}"/VBoxDDU.dylib \
         "${INSTALL_DIR}"/VBoxVMM.dylib \
         "${INSTALL_DIR}"/VBoxRT.dylib \
         "${INSTALL_DIR}"/VBoxDD2.dylib \
         "${INSTALL_DIR}"/VBoxREM.dylib \
        -o "${outfile}" \
        -I"${incdir}" -I"/usr/local/include/fuse" \
        -Wl,-rpath,"${INSTALL_DIR}"  \
        -lfuse_ino64  \
        -Wall ${CFLAGS}