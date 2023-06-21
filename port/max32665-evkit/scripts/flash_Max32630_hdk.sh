fw_file=$1
DIR=$(dirname $(readlink -f $0))
CFG_FILE=$DIR/max3263x_hdk.cfg
openocd -f $CFG_FILE -c "program $fw_file verify reset exit"
