#!/bin/bash

dd if=/dev/zero of=max_size_file.txt bs=4096 count=31

./mkfs -f test.img -d 64 -i 64

./ds3bits test.img

echo -e ">>> Creating file called dummy.txt in root"
./ds3touch test.img 0 dummy.txt
./ds3bits test.img
./ds3ls test.img /

# echo ">>> Creating folders........."
# Loop to create directories from f1 to f62, all filled
# for i in $(seq 1 61); do
#   dirName="f$i"
#   ./ds3mkdir test.img 0 "$dirName"
# done

echo -e ">>> Before writing"
./ds3bits test.img

echo -e ">>> Copying max_size_file.txt into dummy.txt"
./ds3cp test.img max_size_file.txt 1
./ds3bits test.img
./ds3cat test.img 1

# Remove the created files
rm max_size_file.txt