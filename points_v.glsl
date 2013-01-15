//#line 1 "points_v.glsl"
#version 130

uniform float exposure;
uniform float contrast;
uniform float minPointSize;
uniform float maxPointSize;
uniform float pointSize;
in float intensity;
in vec3 position;

void main()
{
    // Point position & size
    vec4 eyeCoord = gl_ModelViewMatrix * vec4(position,1.0);
    gl_Position = gl_ProjectionMatrix * eyeCoord;
    gl_PointSize = clamp(20.0*pointSize / (-eyeCoord.z), minPointSize, maxPointSize);
    // Compute vertex color
    float Y = exposure*pow(intensity/400.0, contrast);
    Y = Y / (1.0 + Y);
    //float Y = clamp(exposure*intensity/400.0, 0.0, 1.0);
    //float Y = clamp(log(exposure*intensity), 0.0, 1.0);
    gl_FrontColor = vec4(Y);
}
