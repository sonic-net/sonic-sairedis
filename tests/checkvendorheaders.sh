#!/bin/bash

# if sairedis is compiled against real sai we will check if vendor headers
# match SAI headers from sairedis submodule, if so, local copy of SAI/inc
# SAI/experimental needs to be created and checked against that

# TODO this may require updates if vendor headers also include experimental

set -e

VENDORDIR=/usr/include/sai

DIR=$(dirname "${BASH_SOURCE[0]}")

if [ -d $VENDORDIR ];
then
    perl -I $DIR/../SAI/meta/ $DIR/../SAI/meta/checkheaders.pl $DIR/../SAI/inc/ $VENDORDIR
else
    echo "Vendor directory $VENDORDIR don't exists, skipping headers check"
fi
