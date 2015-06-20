# Script to generate a regular grid of lines in ply format for testing

f = open("grid.ply", "w")

N = 7

write(f, """
ply
format ascii 1.0
element vertex $(N^3)
property float x
property float y
property float z
element color $(N^3)
property float r
property float g
property float b
element edge $(3*N^2*(N-1))
property list uchar int vertex_index
end_header
""")


# Vertex positions
for i=1:N
    for j=1:N
        for k=1:N
            write(f, "$i $j $k\n")
        end
    end
end

# Vertex colors
for i=1:N
    for j=1:N
        for k=1:N
            r = rand()
            gb = rand()
            if gb > r
                gb = r
            end
            write(f, "$r $gb $gb\n")
        end
    end
end

# Edges
strides = [1,N,N*N]
for axis=0:2
    for i=1:N, j=1:N, k=1:N-1
        i1 = sum(strides .* (circshift([i,j,k], axis) - 1))
        i2 = sum(strides .* (circshift([i,j,k+1], axis) - 1))
        write(f, "2 $i1 $i2\n")
    end
end

close(f)
