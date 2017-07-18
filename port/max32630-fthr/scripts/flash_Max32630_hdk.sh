file="./build/max3263x.elf"
openocd -f scripts/max3263x_hdk.cfg -c "program $file verify reset exit"

