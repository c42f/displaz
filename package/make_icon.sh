#!/bin/bash

make_icon () {
    width=$1
    name="tmp_icon_$(printf "%.3d" $width).png"
    echo "Saving icon to $name" 1>&2
    inkscape \
        --export-png=$name \
        --export-area-page \
        --export-width=$width \
        icon.svg 1>&2
    echo $name
}

echo "Making windows .ico" 1>&2
convert $(make_icon 16) $(make_icon 32) $(make_icon 48) $(make_icon 256) displaz.ico

echo "Making OSX .icns" 1>&2
png2icns displaz.icns $(make_icon 16) $(make_icon 32) $(make_icon 128) $(make_icon 256)

echo "Making linux icon .png" 1>&2
cp $(make_icon 256) ../src/resource/displaz_icon_256.png

rm -f tmp_icon_*.png
