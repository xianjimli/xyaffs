#!/bin/bash

SRC_DIR=system
DST_DIR=system.new
SRC_FILE=system.bin
DST_FILE=system.new.bin

if [ ! -d $SRC_DIR ]
then
	echo Please copy some files into ./$SRC_DIR
	exit 1
fi

echo Testing brn_xyaffs2
rm -rf $DST_DIR
./brn_mkyaffs2image $SRC_DIR $SRC_FILE >/dev/null
./brn_xyaffs2 $SRC_FILE $DST_DIR >/dev/null
diff -r $SRC_DIR $DST_DIR

echo Testing xyaffs2
rm -rf $DST_DIR
./mkyaffs2image $SRC_DIR $SRC_FILE >/dev/null
./xyaffs2 $SRC_FILE $DST_DIR >/dev/null
diff -r $SRC_DIR $DST_DIR

