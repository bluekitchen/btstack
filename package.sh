#!/bin/sh

PACKAGE=BTstack

svn update

VERSION=0.1
REVISION=`svn info | grep Revision | cut -d " " -f 2`
ARCHIVE=$PACKAGE-$VERSION-$REVISION.deb

echo Creating $PACKAGE package version $VERSION revision $REVISION
sudo rm -rf $PACKAGE

mkdir -p $PACKAGE/DEBIAN
cp resources/control $PACKAGE/DEBIAN
echo "Version: $VERSION-$REVISION" >> $PACKAGE/DEBIAN/control

mkdir -p $PACKAGE/usr/local/bin
cp src/BTdaemon $PACKAGE/usr/local/bin
cp PatchBlueTool/PatchBlueTool $PACKAGE/usr/local/bin
cp example/inquiry $PACKAGE/usr/local/bin

mkdir -p $PACKAGE/usr/local/lib
cp src/libBTstack.dylib $PACKAGE/usr/local/lib

mkdir -p $PACKAGE/System/Library/CoreServices/SpringBoard.app
cp resources/*.png $PACKAGE/System/Library/CoreServices/SpringBoard.app

mkdir -p $PACKAGE/Library/LaunchDaemons/
cp resources/ch.ringwald.BTstack.plist $PACKAGE/Library/LaunchDaemons/

# preinst: stop daemon
echo "#!/bin/sh" >  $PACKAGE/DEBIAN/preinst
echo "/bin/launchctl unload /Library/LaunchDaemons/ch.ringwald.BTstack.plist 2&> /dev/null" >> $PACKAGE/DEBIAN/preinst
chmod +x $PACKAGE/DEBIAN/preinst

# regular install

# extrainst_ (cydia only): patch BlueTool and install as /usr/local/bin/BlueToolH4
echo "#!/bin/sh" >  $PACKAGE/DEBIAN/extrainst_
echo "rm -f /tmp/BlueToolH4 /usr/local/bin/BlueToolH4"                >> $PACKAGE/DEBIAN/extrainst_
echo "cp /usr/sbin/BlueTool /tmp/BlueToolH4"                          >> $PACKAGE/DEBIAN/extrainst_
echo "/usr/local/bin/PatchBlueTool /tmp/BlueToolH4  2&> /dev/null"    >> $PACKAGE/DEBIAN/extrainst_
echo "ldid -s /tmp/BlueToolH4"                                        >> $PACKAGE/DEBIAN/extrainst_
echo "cp -f /tmp/BlueToolH4 /usr/local/bin"                           >> $PACKAGE/DEBIAN/extrainst_
echo "rm -f /tmp/BlueToolH4"                                          >> $PACKAGE/DEBIAN/extrainst_
chmod +x $PACKAGE/DEBIAN/extrainst_

# postinst: startup daemon
echo "#!/bin/sh" >  $PACKAGE/DEBIAN/postinst
echo "/bin/launchctl load   /Library/LaunchDaemons/ch.ringwald.BTstack.plist 2&> /dev/null" >> $PACKAGE/DEBIAN/postinst
chmod +x $PACKAGE/DEBIAN/postinst

# prerm: stop daemon and get rid of BlueToolH4 
# note: cannot delete in postrm as it would delete the tool just created by extrainst_
echo "#!/bin/sh" >  $PACKAGE/DEBIAN/prerm
echo "/bin/launchctl unload /Library/LaunchDaemons/ch.ringwald.BTstack.plist 2&> /dev/null" >> $PACKAGE/DEBIAN/prerm
echo "rm -f /usr/local/bin/BlueToolH4" >> $PACKAGE/DEBIAN/prerm
chmod +x $PACKAGE/DEBIAN/prerm

# set ownership to root:root
sudo chown -R 0:0 $PACKAGE

echo Packaging $PACKAGE
export COPYFILE_DISABLE
export COPY_EXTENDED_ATTRIBUTES_DISABLE
dpkg-deb -b $PACKAGE $ARCHIVE
dpkg-deb --info $ARCHIVE
dpkg-deb --contents $ARCHIVE
