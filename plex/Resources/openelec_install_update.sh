#!/bin/sh

# we need to exit with error if something goes wrong
set -ex

UPDATEFILE=$1
EXTRACTPATH=/storage/.update/tmp
INSTALLPATH=/storage/.update
POST_UPDATE_PATH=/storage/.post_update.sh
TTL=5000 # how long to show notifications

post_update()
{
  if [ -f "$POST_UPDATE_PATH" ]; then
    chmod +x $POST_UPDATE_PATH 
    /bin/sh $POST_UPDATE_PATH
  fi
}

notify()
{
  if [ -n "$3" ];then
    ttl=$3
  else
    ttl=$TTL
  fi
  /usr/bin/xbmc-send --port 9778 --action="Notification('$1', '$2', $ttl)"
}

abort()
{
  notify 'Update Aborted!' $1
  exit 1
}

trap abort 1 2 3 6



# first we make sure to create the update path:

if [ ! -d $EXTRACTPATH ]; then
	mkdir -p $EXTRACTPATH
fi

if [ ! -d $INSTALLPATH ]; then
	mkdir -p $INSTALLPATH
fi

notify 'Updating...' 'Beginning extraction, this will take a few minutes.'

# untar both SYSTEM and KERNEL into extraction directory
tar -xf $UPDATEFILE -C $EXTRACTPATH &
tarpid=$!
while [ -n "`ps | grep $tarpid | grep -v 'grep'`" ];do
  sleep 5
  notify 'Updating...' 'Extraction in progress... please wait.'
done

notify 'Updating...' 'Extraction complete, performing checks.'

CONTENTS=`find $EXTRACTPATH`

# Grab KERNEL and SYSTEM 
KERNEL=$(echo $CONTENTS | tr " " "\n" | grep KERNEL$)
SYSTEM=$(echo $CONTENTS | tr " " "\n" | grep SYSTEM$)
KERNELMD5=$(echo $CONTENTS | tr " " "\n"  | grep KERNEL.md5)
SYSTEMMD5=$(echo $CONTENTS | tr " " "\n" | grep SYSTEM.md5)
set +e
POST_UPDATE=$(echo $CONTENTS | tr " " "\n" | grep post_update.sh)
set -e

[ -z "$KERNEL" ] && abort 'Invalid archive - no kernel.'
[ -z "$KERNELMD5" ] && abort 'Invalid archive - no kernel check.'
[ -z "$SYSTEM" ] && abort 'Invalid archive - no system.'
[ -z "$SYSTEMMD5" ] && abort 'Invalid archive - no system check.'
cd $INSTALLPATH

notify 'Updating...' 'Finished extraction, validating checksums.'

if [ -n $POST_UPDATE ] && [-f "$POST_UPDATE" ];then
  notify 'Running post update script'
  cp $POST_UPDATE $POST_UPDATE_PATH
  post_update
  notify 'Post-update complete!'
fi

kernel_check=`/bin/md5sum $KERNEL | awk '{print $1}'`
system_check=`/bin/md5sum $SYSTEM | awk '{print $1}'`

kernelmd5=`cat $KERNELMD5 | awk '{print $1}'`
systemmd5=`cat $SYSTEMMD5 | awk '{print $1}'`

[ "$kernel_check" != "$kernelmd5" ] && abort 'Kernel checksum mismatch'
[ "$system_check" != "$systemmd5" ] && abort 'System checksum mismatch'

notify 'Updating...' 'Checksums valid! Cleaning up...'
# move extracted files to the toplevel
mv $KERNEL $SYSTEM $KERNELMD5 $SYSTEMMD5 .

# remove the directories created by tar
rm -r */
rm $UPDATEFILE
