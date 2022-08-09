#!/bin/bash

echo Checkig for white spaces ...

find .. -type f |
grep -v SAI/ |
grep -v debian/ |
grep -v _wrap.cpp |
perl -ne 'print if /\.(c|cpp|h|hpp|am|sh|pl|pm|install|dirs|links|json|ini|yml|pws|md|py|cfg|conf|i|ac)$/' |
xargs grep -P "\\s\$"

if [ $? -eq 0 ]; then
    echo ERROR: some files contain white spaces at the end of line, please fix
    exit 1
fi
