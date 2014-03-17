module Displaz

# Quick and nasty matlab-like plotting interface
export plot, clf, hold


# Convert julia array into a type name and type appropriate for putting in the
# ply header
ply_type_convert(a::AbstractArray{Uint8})    = ("uint8",   a)
ply_type_convert(a::AbstractArray{Uint16})   = ("uint16",  a)
ply_type_convert(a::AbstractArray{Uint32})   = ("uint32",  a)
ply_type_convert(a::AbstractArray{Int8})     = ("int8",    a)
ply_type_convert(a::AbstractArray{Int16})    = ("int16",   a)
ply_type_convert(a::AbstractArray{Int32})    = ("int32",   a)
ply_type_convert(a::AbstractArray{Float32})  = ("float32", a)
ply_type_convert(a::AbstractArray{Float64})  = ("float64", a)
# Generic cases - actually do a conversion
ply_type_convert{T<:Unsigned}(a::AbstractArray{T}) = ("uint32", uint32(a))
ply_type_convert{T<:Integer }(a::AbstractArray{T}) = ("int32", int32(a))
ply_type_convert{T<:Real    }(a::AbstractArray{T}) = ("float64", float64(a))


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
function write_ply_points(fileName, nvertices, fields)
    convertedFields = [ply_type_convert(value) for (_,__,value) in fields]
    open(fileName, "w") do fid
        write(fid,
            """
            ply
            format binary_little_endian 1.0
            comment Displaz native
            """
        )
        for ((name,semantic,_), (typeName, value)) in zip(fields, convertedFields)
            n = size(value,1)
            assert(n == nvertices || n == 1)
            write(fid, "element vertex_$name $n\n")
            for i = 1:size(value,2)
                propName = ply_property_name(semantic, i)
                write(fid, "property $typeName $propName\n")
            end
        end
        write(fid, "end_header\n")
        for (_,value) in convertedFields
            write(fid, value')
        end
    end
end


#const standard_elements = [:position  => (vector_semantic,3),
#                           :color     => (color_semantic,3),
#                           :marksize  => (array_semantic,1),
#                           :markshape => (array_semantic,1)]

const color_names = ['r' => [1.0  0   0],
                     'g' => [0.0  0.8 0],
                     'b' => [0.0  0   0.8],
                     'c' => [0.0  1  1],
                     'm' => [1.0  0  1],
                     'y' => [1.0  1  0],
                     'k' => [0.0  0  0],
                     'w' => [1.0  1  1]]

interpret_color(color) = color
interpret_color(s::String) = length(s) == 1 ? interpret_color(s[1]) : error("Unknown color abbreviation $s")
interpret_color(c::Char) = color_names[c]


# True if plots are to be plotted over the previous data, false to clear before
# plotting new data sets.
_hold = false


# Basic 3D plotting function for points
function plot(position; color=[1 1 1], markersize=[0.1], markershape=[1])
    color = interpret_color(color)
    size(position, 2) == 3 || error("position must be a Nx3 array")
    size(color, 2)    == 3 || error("color must be a Nx3 array")
    nvertices = size(position, 1)
    # FIXME in displaz itself.  This waste shouldn't be required.
    if size(color,1) == 1
        color = repmat(color, nvertices, 1)
    end
    if size(markersize,1) == 1
        markersize = repmat(markersize, nvertices, 1)
    end
    if size(markershape,1) == 1
        markershape = repmat(markershape, nvertices, 1)
    end
    # Ensure all fields are floats for now, to avoid surprising scaling in the
    # shader
    color = float32(color)
    markersize = float32(markersize)
    markershape = float32(markershape)
    size(color,1) == nvertices || error("color must have same number of rows as position array")
    #fileName = "_julia_tmp.ply"
    fileName = tempname()*".ply"
    write_ply_points(fileName, nvertices, (
                     (:position, vector_semantic, position),
                     (:color, color_semantic, color),
                     (:markersize, array_semantic, markersize),
                     (:markershape, array_semantic, markershape),
                     ))
    holdStr = _hold ? "-add" : ""
    @async run(`displaz $holdStr -shader generic_points.glsl -rmtemp $fileName`)
    #print("displaz $holdStr -shader generic_points.glsl -rmtemp $fileName\n")
    nothing
end


function clf()
    @async run(`displaz -shader generic_points.glsl -clear`)
    nothing
end


function hold()
    global _hold
    _hold = !_hold
end
function hold(h)
    global _hold
    _hold = bool(h)
end


end
