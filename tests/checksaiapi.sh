#!/bin/bash

# this script checks if only allowed files are using global sai apis

# in entire project we only allow VendorSai.cpp and saisdkdump.cpp to use global SAI API,

# PLEASE DO NOT ADD ANY MORE EXCEPTIONS

# we want to keep track of usage global apis to minimize scope of usage for
# possible future mistakes that we will be using dynmically loaded libsai.so
# and by mistake someone will be calling global api that was linked with syncd,
# this will be hard error to locate

set -e

cd ..

find -name "*.o" |
grep -v pyext |
grep -v tests |
while read all;
do
    echo -n $all;
    nm $all |
    grep "U sai_" |
    grep -vP "sai_metadata|sai_serialize|sai_deserialize" |
    perl -npe 'chomp'
    echo
done |
grep "U sai_" |
awk '{print $1}' |
perl -ne 'chomp; die "file $_ is using global sai_xxx API, please correct your code" if not /VendorSai.o|saisdkdump/'

REGEX=`cat SAI/meta/saimetadata.c|grep dlsym|grep handle|perl -ne 'print "$1|" if /(sai_\w+)/'|perl -pe 'chop'|perl -ne 'print "\\\\b($_)\\\\b"'`

set +e
find -name "*.cpp" -o -name "*.c" |
xargs grep -P "$REGEX" |
grep -vP "/unittest/|/tests/|/SAI/|/pyext/|tests.cpp|sai_vs_interfacequery|sai_proxy_interfacequery|sai_redis_interfacequery|saisdkdump|SWSS_LOG|.cpp:\s+\*|.cpp:\s+//|sai_status_t\s+sai_|VendorSai.cpp:.+=\s*&sai_"

if [ $? == 0 ]; then
    echo not allowed files are using global sai_xxx API, please correct your code, only VendorSai.cpp and saisdkdump are allowed to use global SAI apis
    exit 1
fi

