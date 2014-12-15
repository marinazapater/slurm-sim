#!/bin/bash

if [ "x$SLURMSOCKHOST" == "x" ]; then
    echo "slurm socket host envvar not set";
    exit -1;
fi;
if [ "x$SLURMSOCKPORT" == "x" ]; then
    echo "slurm socket port envvar not set";
    exit -1;
fi;
if [ "x$DCSIMUSER" == "x" ]; then
    echo "DC sim user envvar not set";
    exit -1;
fi;
if [ "x$DCSIMHOST" == "x" ]; then
    echo "DC sim host envvar not set";
    exit -1;
fi;
if [ "x$DCSIMPORT" == "x" ]; then
    echo "DC sim port envvar not set";
    exit -1;
fi;

exit 0;

