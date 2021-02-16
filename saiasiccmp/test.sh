#!/bin/bash

./saiasiccmp dump1.json dump2.json

if [ $? != 0 ]; then
    echo "ERROR: expected dumps do be equal"
    exit 1
fi

