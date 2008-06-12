#!/bin/bash
cd ../PORT/bt-st
rm *.o *.a
make
cd ../../btree-st
rm test.bt
rm bench
rm btrecord
rm *.o
make -f Makefile.bench
./bench &> /tmp/bench
tail /tmp/bench
