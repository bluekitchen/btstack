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
cp resources/InstallBlueToolNoH5.sh $PACKAGE/usr/local/bin
cp example/inquiry $PACKAGE/usr/local/bin

mkdir -p $PACKAGE/usr/local/lib
cp src/libBTstack.dylib $PACKAGE/usr/local/lib

mkdir -p $PACKAGE/System/Library/CoreServices/SpringBoard.app
cp resources/*.png $PACKAGE/System/Library/CoreServices/SpringBoard.app

mkdir -p $PACKAGE/Library/LaunchDaemons/
cp resources/ch.ringwald.BTstack.plist $PACKAGE/Library/LaunchDaemons/

echo "#!/bin/sh" >  $PACKAGE/DEBIAN/postinst
echo "/bin/launchctl unload /Library/LaunchDaemons/ch.ringwald.BTstack.plist 2&> /dev/null" >> $PACKAGE/DEBIAN/postinst
echo "/bin/launchctl load   /Library/LaunchDaemons/ch.ringwald.BTstack.plist" >> $PACKAGE/DEBIAN/postinst
chmod +x $PACKAGE/DEBIAN/postinst
echo "#!/bin/sh" >  $PACKAGE/DEBIAN/prerm
echo "/bin/launchctl unload /Library/LaunchDaemons/ch.ringwald.BTstack.plist" >> $PACKAGE/DEBIAN/prerm
chmod +x $PACKAGE/DEBIAN/prerm

# set ownership to root:root
sudo chown -R 0:0 $PACKAGE

# set suid for InstallBlueToolNoH5
sudo chmod 4755 $PACKAGE/usr/local/bin/InstallBlueToolNoH5.sh

echo Packaging $PACKAGE
export COPYFILE_DISABLE
export COPY_EXTENDED_ATTRIBUTES_DISABLE
dpkg-deb -b $PACKAGE $ARCHIVE
dpkg-deb --info $ARCHIVE
dpkg-deb --contents $ARCHIVE
