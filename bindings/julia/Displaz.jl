module Displaz
using Compat

# Quick and nasty matlab-like plotting interface
export plot3d, plot3d!, plot, clf, hold


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
            n = size(value,2)
            assert(n == nvertices || n == 1)
            write(fid, "element vertex_$name $n\n")
            for i = 1:size(value,1)
                propname = ply_property_name(semantic, i)
                write(fid, "property $typename $propname\n")
            end
        end
        write(fid, "end_header\n")
        for (_,value) in converted_fields
            write(fid, value)
        end
    end
end

function write_ply_lines(filename, position, color, linebreak)
    nvalidvertices = size(position,2)

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

    write(fid,convert(Array{Float64,2},position))
    write(fid,color)

    realstart = 0
    linelen = []
    range = []
    for i = 1:size(linebreak,1)
        if i != size(linebreak,1)
            linelen = linebreak[i+1] - linebreak[i]
            range = realstart:realstart + linelen-1
        else
            linelen = size(position,2) - linebreak[i] + 1
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

const _color_names = @compat Dict('r' => [1.0, 0,   0],
                                  'g' => [0.0, 0.8, 0],
                                  'b' => [0.0, 0,   0.8],
                                  'c' => [0.0, 1,   1],
                                  'm' => [1.0, 0,   1],
                                  'y' => [1.0, 1,   0],
                                  'k' => [0.0, 0,   0],
                                  'w' => [1.0, 1,   1])

const _shape_ids = @compat Dict('.' => 0,
                                's' => 1,
                                'o' => 2,
                                'x' => 3,
                                '+' => 4,
                                '-' => '-')

interpret_color(color) = color
interpret_color(s::AbstractString) = length(s) == 1 ? interpret_color(s[1]) : error("Unknown color abbreviation $s")
interpret_color(c::Char) = _color_names[c]
interpret_shape(markershape) = markershape
interpret_shape(c::Char) = [_shape_ids[c]]
interpret_shape(s::Vector{Char}) = Int[_shape_ids[c] for c in s]
interpret_shape(s::AbstractString) = Int[_shape_ids[c] for c in s]
interpret_linebreak(nvertices, linebreak) = linebreak
interpret_linebreak(nvertices, i::Integer) = i == 1 ? [1] : 1:i:nvertices

# Multiple figure window support
# TODO: Consider how the API below relates to Plots.jl and its tendency to
# create a lot of new figure windows rather than clearing existing ones.
type DisplazWindow
    name::AbstractString
end

_current_figure = DisplazWindow("default")
"Get handle to current figure window"
function current()
    _current_figure
end

"Get figure window by name may be new or already existing"
function figure(name::AbstractString)
    global _current_figure
    _current_figure = DisplazWindow(name)
end
"Get figure window with given id"
figure(id::Integer) = figure("Figure $id")

_figure_id = 1
"Create next incrementally named figure window, counting automatically from \"Figure 1\""
function newfigure()
    global _figure_id
    id = _figure_id
    _figure_id += 1
    figure(id)
end


"""
Plot 3D points or lines on a new plot

```
  plot3d([plotobj,] position; attr1=value1, ...)
```

The `position` array should be a 3xN array of vertex positions. (This is
consistent with treating the components of a single point as a column vector.
It also has the same memory layout as an array of short `Vector3` instances
from ImmutableArrays, for example).

The `plotobj` argument is optional and determines which plot window to send the
data to.  If it's not used the data will be sent to the plot window returned
by `current()`.

TODO: Revisit the nasty decision of the shape of position again - the above
choice is somewhat inconsistent with supplying markersize / markershape as a
column vector :-(  Can we have a set of consistent broadcasting rules for this?
It seems like the case of a 3x3 matrix will always be ambiguous if we try
to guess what the user wants.

### Data set attributes

The following attributes can be attached to a dataset on each call to `plot3d`:

  * `label` - A string labeling the data set

### Vertex attributes

Each point may have a set of vertex attributes attached to control the visual
representation and tag the point for inspection. The following are supported by
the default shader:

  * `color`       - A 3xN array of vertex colors
  * `markersize`  - Vertex marker size
  * `markershape` - Vector of vertex marker shapes.  Shape can be represented
                    either by a Char or a numeric identifier between 0 and 4:

```
                    sphere - '.' 0    square - 's' 1
                    circle - 'o' 2    times  - 'x' 3
                    cross  - '+' 4
```


### Plotting points

To plot 10000 random points with distance from the origin determining the
color, and random marker shapes:
```
  P = randn(3,10000)
  c = 0.5./sumabs2(P,1) .* [1,0,0]
  plot3d(P, color=c, markershape=rand(1:4,10000))
```


### Plotting lines

To plot a piecewise linear curve between 10000 random vertices
```
  plot3d(randn(3,10000), markershape="-")
```

When plotting lines, the `linebreak` keyword argument can be used to break the
position array into multiple line segments.  Each index in the line break array
is the initial index of a line segment.
"""
function plot3d(plotobj::DisplazWindow, position; color=[1,1,1], markersize=[0.1], markershape=[0],
                label=nothing, linebreak=[1], _clear_before_plot=true)
    nvertices = size(position, 2)
    color = interpret_color(color)
    markershape = interpret_shape(markershape)
    linebreak = interpret_linebreak(nvertices, linebreak)
    size(position, 1) == 3 || error("position must be a 3xN array")
    size(color, 1)    == 3 || error("color must be a 3xN array")
    # FIXME in displaz itself.  No repmat waste should be required.
    if size(color,2) == 1
        color = repmat(color, 1, nvertices)
    end
    # Ensure all fields are floats for now, to avoid surprising scaling in the
    # shader
    color = map(Float32,color)
    markersize = map(Float32,markersize)
    size(color,2) == nvertices || error("color must have same number of rows as position array")
    filename = tempname()*".ply"
    seriestype = "Points"
    if '-' in markershape # Plot lines
        seriestype = "Line"
        write_ply_lines(filename, position, color, linebreak)
    else # Plot points
        if length(markersize) == 1
            markersize = repmat(markersize, 1, nvertices)
        end
        if length(markershape) == 1
            markershape = repmat(markershape, 1, nvertices)
        end
        write_ply_points(filename, nvertices, (
                         (:position, vector_semantic, position),
                         (:color, color_semantic, color),
                         # FIXME: shape of markersize??
                         (:markersize, array_semantic, vec(markersize)'),
                         (:markershape, array_semantic, vec(markershape)'),
                         ))
    end
    if label === nothing
        label = "$seriestype [$nvertices vertices]"
    end
    addopt = _clear_before_plot ? [] : "-add"
    run(`displaz -script $addopt -server $(plotobj.name) -dataname $label -shader generic_points.glsl -rmtemp $filename`)
    nothing
end


"""
Add points or lines to an existing 3D plot

See plot3d for documentation
"""
function plot3d!(plotobj::DisplazWindow, position; kwargs...)
    plot3d(plotobj, position; _clear_before_plot=false, kwargs...)
end


# Plot to current window
plot3d!(position; kwargs...) = plot3d!(current(), position; kwargs...)
plot3d(position; kwargs...)  = plot3d(current(), position; kwargs...)


#-------------------------------------------------------------------------------
# Deprecated junk

_hold = false

function plot(position; color=[1 1 1], markersize=[0.1], markershape=[0], linebreak = [1])
    Base.depwarn("plot() is deprecated.  Use plot3d() instead", :plot)
    plot3d(position', color=color', markersize=markersize, markershape=markershape, linebreak=linebreak, _clear_before_plot=!_hold)
end

function hold()
    Base.depwarn("hold() is deprecated.  Use plot3d() to make a new plot, and plot3d!() to add to an existing one", :hold)
    global _hold
    _hold = !_hold
end

function hold(h)
    Base.depwarn("hold() is deprecated.  Use plot3d() to make a new plot, and plot3d!() to add to an existing one", :hold)
    global _hold
    _hold = Bool(h)
end

function clf()
    Base.depwarn("clf() is deprecated.  Use plot3d() to make a new plot, and plot3d!() to add to an existing one", :clf)
    run(`displaz -clear`)
    nothing
end

end
