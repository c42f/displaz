#version 130

uniform float exposure = 1.0;     //# uiname=Exposure; min=0.01; max=10000
uniform float contrast = 1.0;     // uiname=Contrast; min=0.01; max=10000
uniform float trimRadius = 10000;  //# uiname=Trim Radius; min=1; max=10000
uniform float minPointSize = 0;
uniform float maxPointSize = 100.0;
uniform float pointSize = 10.0;    //# uiname=Point Size; min=1; max=200
uniform vec3 cursorPos = vec3(0);
uniform int selector = 0;         //# uiname=File Selector; min=-1; max=100
uniform int colorMode = 0;         //# uiname=Colour Mode; min=0; max=1
uniform int fileNumber = 0;
in float intensity;
in vec3 position;
in vec3 color;

flat out float pointScreenSize;
flat out vec4 pointColor;
flat out int markerShape;

void main()
{
    // Point position & size
    vec4 eyeCoord = gl_ModelViewMatrix * vec4(position,1.0);
    gl_Position = gl_ProjectionMatrix * eyeCoord;
    float r = length(position.xy - cursorPos.xy)/trimRadius;
    pointScreenSize = clamp(20.0*pointSize / (-eyeCoord.z) * (1 - r*r*r), minPointSize, maxPointSize);
    gl_PointSize = pointScreenSize;
    // Compute vertex color
    if (colorMode == 1)
        pointColor = vec4(exposure*color,1);
    else
    {
        float Y = exposure*pow(intensity/400.0, contrast);
        Y = Y / (1.0 + Y);
        pointColor = Y*vec4(1);
    }
    if (selector == fileNumber)
        markerShape = 0;
    else
        markerShape = -1;
}

