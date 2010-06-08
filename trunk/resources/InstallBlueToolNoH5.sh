#!/bin/sh
rm -f /tmp/BlueToolNoH5
cp /usr/sbin/BlueTool /tmp/BlueToolNoH5
/usr/local/bin/PatchBlueTool /tmp/BlueToolNoH5
ldid -S /tmp/BlueToolNoH5
cp -f /tmp/BlueToolNoH5 /usr/local/bin
rm -f /tmp/BlueToolNoH5
