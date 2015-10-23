module Displaz
using Compat

# Quick and nasty matlab-like plotting interface
export plot, clf, hold


# Convert julia array into a type name and type appropriate for putting in the
# ply header
ply_type_convert(a::AbstractArray{UInt8})    = ("uint8",   a)
ply_type_convert(a::AbstractArray{UInt16})   = ("uint16",  a)
ply_type_convert(a::AbstractArray{UInt32})   = ("uint32",  a)
ply_type_convert(a::AbstractArray{Int8})     = ("int8",    a)
ply_type_convert(a::AbstractArray{Int16})    = ("int16",   a)
ply_type_convert(a::AbstractArray{Int32})    = ("int32",   a)
ply_type_convert(a::AbstractArray{Float32})  = ("float32", a)
ply_type_convert(a::AbstractArray{Float64})  = ("float64", a)
# Generic cases - actually do a conversion
ply_type_convert{T<:Unsigned}(a::AbstractArray{T}) = ("uint32",  map(UInt32,a))
ply_type_convert{T<:Integer }(a::AbstractArray{T}) = ("int32",   map(Int32,a))
ply_type_convert{T<:Real    }(a::AbstractArray{T}) = ("float64", map(Float64,a))


const array_semantic = 0
const vector_semantic = 1
const color_semantic = 2

# get ply header property name for given field index and semantic
function ply_property_name(semantic, idx)
    if semantic == array_semantic
        string(idx-1)
    elseif semantic == vector_semantic
        ("x", "y", "z", "w")[idx]
    elseif semantic == color_semantic
        ("r", "g", "b")[idx]
    end
end

# Write a set of points to displaz-native ply format
function write_ply_points(filename, nvertices, fields)
    converted_fields = [ply_type_convert(value) for (_,__,value) in fields]
    open(filename, "w") do fid
        write(fid, "ply\n")
        write(fid, "format binary_little_endian 1.0\n")
        write(fid, "comment Displaz native\n")
        for ((name,semantic,_), (typename, value)) in zip(fields, converted_fields)
            n = size(value,1)
            assert(n == nvertices || n == 1)
            write(fid, "element vertex_$name $n\n")
            for i = 1:size(value,2)
                propname = ply_property_name(semantic, i)
                write(fid, "property $typename $propname\n")
            end
        end
        write(fid, "end_header\n")
        for (_,value) in converted_fields
            write(fid, value')
        end
    end
end

function write_ply_lines(filename, position, color, linebreak)
    nvalidvertices = size(position,1)

    # Create and write to ply file
    fid = open(filename, "w")
    write(fid, 
        """
        ply
        format binary_little_endian 1.0
        element vertex $nvalidvertices
        property double x
        property double y
        property double z
        element color $nvalidvertices
        property float r
        property float g
        property float b
        element edge $(length(linebreak))
        property list int int vertex_index
        end_header
        """
    )

    write(fid,convert(Array{Float64,2},position'))
    write(fid,color')
    
    realstart = 0
    linelen = []
    range = [] 
    for i = 1:size(linebreak,1)
        if i != size(linebreak,1)
            linelen = linebreak[i+1] - linebreak[i]
            range = realstart:realstart + linelen-1 
        else
            linelen = size(position,1) - linebreak[i] + 1 
            range = realstart:realstart + linelen - 1 
        end
        write(fid,Int32(linelen))
        write(fid,UnitRange{Int32}(range))
        realstart = realstart + linelen
    end
    close(fid)
end

#const standard_elements = [:position  => (vector_semantic,3),
#                           :color     => (color_semantic,3),
#                           :marksize  => (array_semantic,1),
#                           :markshape => (array_semantic,1)]

const color_names = @compat Dict('r' => [1.0  0   0],
                                 'g' => [0.0  0.8 0],
                                 'b' => [0.0  0   0.8],
                                 'c' => [0.0  1  1],
                                 'm' => [1.0  0  1],
                                 'y' => [1.0  1  0],
                                 'k' => [0.0  0  0],
                                 'w' => [1.0  1  1])

const shape_names = @compat Dict('.' => [0],
                                 's' => [1],
                                 'o' => [2],
                                 'x' => [3],
                                 '+' => [4],
                                 '-' => '-')

interpret_color(color) = color
interpret_color(s::AbstractString) = length(s) == 1 ? interpret_color(s[1]) : error("Unknown color abbreviation $s")
interpret_color(c::Char) = color_names[c]
interpret_shape(markershape) = markershape
interpret_shape(c::Char) = shape_names[c]
interpret_linebreak(nvertices, linebreak) = linebreak
interpret_linebreak(nvertices, i::Integer) = i == 1 ? [1] : 1:i:nvertices
# True if plots are to be plotted over the previous data, false to clear before
# plotting new data sets.
_hold = false

# Basic 3D plotting function for points
function plot(position; color=[1 1 1], markersize=[0.1], markershape=[0], linebreak = [1])
    nvertices = size(position, 1)
    color = interpret_color(color)
    markershape = interpret_shape(markershape)
    linebreak = interpret_linebreak(nvertices, linebreak)
    size(position, 2) == 3 || error("position must be a Nx3 array")
    size(color, 2)    == 3 || error("color must be a Nx3 array")
    # FIXME in displaz itself.  This waste shouldn't be required.
    if size(color,1) == 1
        color = repmat(color, nvertices, 1)
    end
    if size(markersize,1) == 1
        markersize = repmat(markersize, nvertices, 1)
    end
    # Ensure all fields are floats for now, to avoid surprising scaling in the
    # shader
    color = map(Float32,color)
    markersize = map(Float32,markersize)
    size(color,1) == nvertices || error("color must have same number of rows as position array")
    filename = tempname()*".ply"
    if '-' in markershape # Plot lines
        write_ply_lines(filename, position, color, linebreak)
    else # Plot points
        if size(markershape,1) == 1
            markershape = repmat(markershape, nvertices, 1)
        end
        write_ply_points(filename, nvertices, (
                         (:position, vector_semantic, position),
                         (:color, color_semantic, color),
                         (:markersize, array_semantic, markersize),
                         (:markershape, array_semantic, markershape),
                         ))
    end
    hold = _hold ? "-add" : []
    run(`displaz -background $hold -shader generic_points.glsl -rmtemp $filename`)
    nothing
end

function clf()
    run(`displaz -clear`)
    nothing
end

function hold()
    global _hold
    _hold = !_hold
end

function hold(h)
    global _hold
    _hold = Bool(h)
end

end
