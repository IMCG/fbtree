#!/bin/bash
cd ../PORT/bt-st
make
cd ../../btree-st
rm test.bt
rm t
make
./t &> /tmp/t
tail /tmp/t
