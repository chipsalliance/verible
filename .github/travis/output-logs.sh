#!/bin/bash

find -L -name \*.log
for F in $(find -L -name \*.log); do
    echo
    export FOLD_NAME=$(echo $F | sed -e's/[^A-Za-z0-9]/_/g')
    travis_fold start $FOLD_NAME
    echo -e "\n\n$F\n--------------"
    cat $F
    echo "--------------"
    travis_fold end $FOLD_NAME
    echo
done
