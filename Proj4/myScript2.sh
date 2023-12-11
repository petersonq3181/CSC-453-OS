#!/bin/bash

rm tinyFSDisk

make 

./tinyFSDemo

hexdump -C tinyFSDisk
