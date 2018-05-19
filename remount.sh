#!/bin/bash
g++ -D_FILE_OFFSET_BITS=64 -o oshfs ./project/main.cpp -lfuse -std=c++14
sudo umount mountpoint
./oshfs -f mountpoint

