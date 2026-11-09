echo "Restarting Bluetooth daemon"

echo "- stop  Bluetooth daemon"
adb shell su root setprop ctl.stop mtkbt

echo "- start Bluetooth daemon"
adb shell su root setprop ctl.start mtkbt
