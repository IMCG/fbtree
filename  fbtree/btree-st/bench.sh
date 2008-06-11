#!/bin/bash
cd ../PORT/bt-st
rm *.o *.a
make
cd ../../btree-st
rm test.bt
rm t
make -f Makefile.bench
./bench &> /tmp/bench
tail /tmp/bench
