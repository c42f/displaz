#version 130

#if defined(VERTEX_SHADER)
uniform vec3 color = vec3(1.0);

in vec3 position;
in vec3 normal;
out vec3 position_eye;

void main()
{
    gl_Position = gl_ModelViewProjectionMatrix * vec4(position,1.0);
    position_eye = (gl_ModelViewMatrix * vec4(position,1.0)).xyz;
    //gl_FrontColor = vec4(color*abs(dot(normal, lightDir)), 1.0);
}


//------------------------------------------------------------------------------
#elif defined(FRAGMENT_SHADER)
uniform vec3 lightDir_eye = vec3(0.0,0.0,-1.0);
in vec3 position_eye;

out vec4 fragColor;

// Faceted shading, without requiring special support in the C++ code.
void main()
{
    vec3 normal = normalize(cross(dFdx(position_eye), dFdy(position_eye)));
    fragColor = vec4(vec3(abs(dot(normal, lightDir_eye))), 1);
}
#endif

