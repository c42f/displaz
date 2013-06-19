#version 130

#if defined(VERTEX_SHADER)
uniform float zoffset = 1e-3;

in vec3 position;
in vec3 color;

void main()
{
    gl_Position = gl_ModelViewProjectionMatrix * vec4(position,1.0);
    gl_Position.z -= zoffset;
    gl_FrontColor = vec4(color, 1.0);
}


//------------------------------------------------------------------------------
#elif defined(FRAGMENT_SHADER)
out vec4 fragColor;

void main()
{
    fragColor = gl_Color;
}
#endif
