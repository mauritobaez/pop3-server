#!/bin/bash

# Como primer argumento se recibe el mailbox.
for i in {0..900}
do
    rm -r "$1/$i"
    mkdir "$1/$i"
    printf ".\r\n aaaaaaaaaaaaaaaaaaaaaa\r\n aaaaa\r\n" > "$1/$i/archivo"
done