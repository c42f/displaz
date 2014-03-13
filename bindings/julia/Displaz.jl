module Displaz

export plot3d

# Convert julia type into a type name appropriate for putting in the ply header
ply_type_name(::Uint8)   = "uint8"
ply_type_name(::Uint16)  = "uint16"
ply_type_name(::Uint32)  = "uint32"
ply_type_name(::Int8)    = "int8"
ply_type_name(::Int16)   = "int16"
ply_type_name(::Int32)   = "int32"
ply_type_name(::Float32) = "float32"
ply_type_name(::Float64) = "float64"


# Basic 3D plotting function for plotting points in displaz
function plot3d(position)
    #fileName = tempname()*".ply"
    fileName = "_julia_tmp.ply"
    positionTypeName = ply_type_name(position[1])
    nvertices = size(position, 1)
    open(fileName, "w") do f
        write(f,
            """
            ply
            format binary_little_endian 1.0
            comment Displaz native
            element vertex_position $nvertices
            property $positionTypeName x
            property $positionTypeName y
            property $positionTypeName z
            end_header
            """
        )
        write(f, position')
        # Write as ascii:
        #for i=1:nvertices
            #@printf(f, "%.3f %.3f %.3f\n", position[i,1], position[i,2], position[i,3])
        #end
    end
    @async run(`displaz $fileName`)
end


end
