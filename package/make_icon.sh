#!/bin/bash

files=""
for width in 16 32 48 256 ; do
    name=tmp_icon_$(printf "%.3d" $width).png
    inkscape \
        --export-png=$name \
        --export-area-page \
        --export-width=$width \
        icon.svg
    files="$files $name"
done

convert $files displaz.ico

cp tmp_icon_256.png ../src/resource/displaz_icon_256.png

rm $files
