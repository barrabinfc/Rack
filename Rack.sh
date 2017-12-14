#!/bin/bash

MODE=$1

# Run doctest ?
if [ $(MODE) = "doctest"]; then
	LD_LIBRARY_PATH=./dep/lib/ ./doctest
fi;

# Plain Rack App
LD_LIBRARY_PATH=./dep/lib/ ./Rack

