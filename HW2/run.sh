#!/bin/bash

# num=1
# var=$( cat ../examples/example${num}_command )
# echo $var
# ./cacheSim "../examples/example${num}_trace" ${var} > 1.txt
# gdb --args ./cacheSim "../examples/example${num}_trace" ${var}

BLACK=`tput setaf 0`
RED=`tput setaf 1`
GREEN=`tput setaf 2`
YELLOW=`tput setaf 3`
BLUE=`tput setaf 4`
MAGENTA=`tput setaf 5`
CYAN=`tput setaf 6`
WHITE=`tput setaf 7`

echo "${CYAN}pray: "
for num in {1..20}
do
    var=$(cat ./tests/example${num}_command)
    echo $var
    ./cacheSim ${var} > ./tests/our_res.txt
    diff ./tests/our_res.txt ./tests/example${num}_output
    if cmp ./tests/our_res.txt ./tests/example${num}_output
        then
    echo "${GREEN}test ${num} passed";
        else
        echo "${RED}test ${num} failed";
    fi
done
echo

