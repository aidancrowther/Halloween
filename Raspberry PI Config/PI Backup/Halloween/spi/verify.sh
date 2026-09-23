#!/bin/bash

for i in {0..500}
do
    ./boundary-new
    sleep 10
    ./disable-new
    sleep 2
done
