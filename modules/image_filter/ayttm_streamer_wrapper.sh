#!/bin/bash

if ! ps -u $UID | grep -q webcam >/dev/null; then
	/usr/bin/webcam &>/dev/null &
fi

IMGDIR=$( sed -ne "/^dir *= */{s///;p
}" ~/.webcamrc )
if ! echo $IMGDIR | grep -q "^/" >/dev/null; then
	IMGDIR=$HOME/$IMGDIR
fi
IMGFILE=$( sed -ne "/^file *= */{s///;p
}" ~/.webcamrc )

cat $IMGDIR/$IMGFILE

exit 0

[ $1 = "-d" ] && DIR=$2 || DIR=~/.ayttm

LOCK=$DIR/webcam_grab.lock
[ -e $LOCK ] && exit 0
touch $LOCK

/usr/bin/streamer -q -f jpeg -o /dev/stdout OUTFILE 2>/dev/null
rm -f $LOCK

exit 0
