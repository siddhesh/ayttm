#!/bin/bash

make_rcfile()
{
	file=$1
	outdir=$2/webcam
	[ ! -e $outdir ] && mkdir $outdir
	cat >$file <<EOF
[ftp]
dir  = $outdir
file = webcam.jpeg
tmp  = uploading.jpeg
local = 1

[grab]
device = /dev/video0
text = "webcam %Y-%m-%d %H:%M:%S"
width = 320
height = 240
delay = 3
quality = 75
EOF

}

RCFILE=~/.webcamrc
[ "$1" = "-d" ] && AYTTM_DIR=$2 || AYTTM_DIR=~/.ayttm

[ ! -e $RCFILE ] && make_rcfile $RCFILE $AYTTM_DIR

if ! ps -u $UID | grep -q webcam >/dev/null; then
	/usr/bin/webcam &>/dev/null &
fi
ps -u $UID | grep -q webcam >/dev/null || exit -1

IMGDIR=$( sed -ne "/.*\<dir *= */{s///;p
}" $RCFILE )
if ! echo $IMGDIR | sed -e 's,^~/,,' | grep -q "^/" >/dev/null; then
	IMGDIR=$HOME/$IMGDIR
fi
IMGFILE=$( sed -ne "/.*\<file *= */{s///;p
}" $RCFILE )

cat $IMGDIR/$IMGFILE || exit -1

exit 0

#LOCK=$AYTTM_DIR/webcam_grab.lock
#[ -e $LOCK ] && exit 0
#touch $LOCK

#/usr/bin/streamer -q -f jpeg -o /dev/stdout OUTFILE 2>/dev/null
#rm -f $LOCK

#exit 0
