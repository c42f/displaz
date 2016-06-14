#!/bin/bash

# A small test script for the displaz event hook system.
#
# Allows the user to draw polygons by moving the 3D cursor, pressing the
# "Space" key to insert a vertex, and Shift+Space to finish a polygon and start
# the next.

set -u

make_polygon() {
    numverts=$1
    shift
    verts=$(echo -n -e "$@")
cat <<EOF
ply
format ascii 1.0
comment Simple tetrahedron
element vertex $numverts
property float x
property float y
property float z
element face 1
property list int int vertex_index
end_header
$verts
$numverts $(seq 0 $((numverts-1)))
EOF
}


i=1
label="Polygon1"
numverts=0
verts=""
displaz -hook key:Space cursor -hook key:Shift+Space key | \
while read event_key payload_type args ; do
    echo $event_key $payload_type $args
    if [[ $event_key == key:Space ]] ; then
        numverts=$((numverts+1))
        verts="$verts\n$args"
        if (( numverts >= 3 )) ; then
            tmp=$(mktemp /tmp/polygon_XXXXXXXXX.ply)
            make_polygon $numverts "$verts" > $tmp
            displaz -script -rmtemp -label $label $tmp
        fi
    fi
    if [[ $event_key != key:Space ]] ; then
        i=$((i+1))
        label="Polygon$i"
        numverts=0
        verts=""
    fi
done

