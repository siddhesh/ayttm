#!/bin/bash

[ ! -x /usr/bin/streamer -o ! -x /usr/bin/ppmtojpeg ] && exit -1

OUTFILE=`mktemp -q /tmp/ayttm-$UID-XXXXXX` || exit -2
/usr/bin/streamer -q -f ppm -o $OUTFILE 2>/dev/null
cat $OUTFILE | /usr/bin/ppmtojpeg
rm -f $OUTFILE

exit 0
