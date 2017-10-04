#/bin/sh

# check esp-idf path
if [ ! -f "$IDF_PATH/README.md" ]; then
	echo "IDF_PATH not set, please follow esp-idf installation instructions"
	echo "http://esp-idf.readthedocs.io/en/latest/get-started/index.html#setup-path-to-esp-idf"
	exit 10
fi

echo "Integrating BTstack into esp-idf at $IDF_PATH"

echo "Adding component/btstack"

# create subsys/btstack
rsync -a components/btstack ${IDF_PATH}/components

# sync sources
rsync -a ../../src ${IDF_PATH}/components/btstack

# sync bludroid
rsync -a ../../3rd-party/bluedroid ${IDF_PATH}/components/btstack/3rd-party

# sync hxcmod player
rsync -a ../../3rd-party/hxcmod-player ${IDF_PATH}/components/btstack/3rd-party

# sync freertos support
rsync -a ../../platform/freertos ${IDF_PATH}/components/btstack/platform

# sync embedded run loop
rsync -a ../../platform/embedded ${IDF_PATH}/components/btstack/platform

# sync tools - used to access compile_gatt.py
rsync -a ../../tool ${IDF_PATH}/components/btstack

# create samples/btstack
./create_examples.py
