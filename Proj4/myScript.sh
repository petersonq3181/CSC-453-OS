#!/bin/bash

rm a.txt 

gcc -g libTinyFS.c libDisk.o 

./a.out 

hexdump -C a.txt 
