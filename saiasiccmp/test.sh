#!/bin/bash

EXIT_VALUE=0

function test_positive()
{
    ./saiasiccmp dump1.json dump2.json

    if [ $? != 0 ]; then
        echo "${FUNCNAME[0]} ERROR: expected dumps to be equal"
        EXIT_VALUE=1
    fi
}

function test_negative()
{
    ./saiasiccmp dump1.json dump3.json

    if [ $? == 0 ]; then
        echo "${FUNCNAME[0]} ERROR: expected dumps to be not equal"
        EXIT_VALUE=1
    fi
}

test_positive;
test_negative;

exit $EXIT_VALUE
