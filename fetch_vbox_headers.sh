#!/bin/sh
SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )" 
svn co http://www.virtualbox.org/svn/vbox/trunk/include/VBox ${SCRIPT_DIR}/include/VBox
