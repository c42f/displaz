#version 130

uniform float pointSize = 10.0;    //# uiname=Point Size; min=1; max=200
uniform float trimRadius = 10000;  //# uiname=Trim Radius; min=1; max=10000
uniform int selector = 0;          //# uiname=File Selector; min=-1; max=100
uniform float exposure = 1.0;      //# uiname=Exposure; min=0.01; max=10000
uniform float contrast = 1.0;      //# uiname=Contrast; min=0.01; max=10000
uniform int colorMode = 0;         //# uiname=Colour Mode; enum=Intensity|Colour|Return Number|Number Of Returns|Point Source
uniform float minPointSize = 0;
uniform float maxPointSize = 100.0;
// Point size multiplier to keep coverage constant when doing stochastic
// simplification
uniform float pointSizeLodMultiplier = 1;
uniform vec3 cursorPos = vec3(0);
uniform int fileNumber = 0;
in float intensity;
in vec3 position;
in vec3 color;
// FIXME: Should avoid turning these two into floats!
in float returnIndex;
in float numberOfReturns;
in float pointSourceId;

flat out float pointScreenSize;
flat out vec3 pointColor;
flat out int markerShape;

float tonemap(float x, float exposure, float contrast)
{
    float Y = exposure*pow(x, contrast);
    Y = Y / (1.0 + Y);
    return Y;
}

void main()
{
    // Point position & size
    vec4 eyeCoord = gl_ModelViewMatrix * vec4(position,1.0);
    gl_Position = gl_ProjectionMatrix * eyeCoord;
    float r = length(position.xy - cursorPos.xy);
    float trimFalloffLen = min(5, trimRadius/2);
    float trimScale = min(1, (trimRadius - r)/trimFalloffLen);
    pointScreenSize = clamp(20.0*pointSize / (-eyeCoord.z) * trimScale * pointSizeLodMultiplier,
                            minPointSize, maxPointSize);
    if (selector != fileNumber)
        pointScreenSize = 0;
    gl_PointSize = pointScreenSize;
    markerShape = 0;
    // Compute vertex color
    if (colorMode == 0)
        pointColor = tonemap(intensity/400.0, exposure, contrast) * vec3(1);
    else if (colorMode == 1)
        pointColor = contrast*(exposure*color - vec3(0.5)) + vec3(0.5);
    else if (colorMode == 2)
        pointColor = returnIndex*51.0*exposure * vec3(1);
    else if (colorMode == 3)
        pointColor = numberOfReturns*51.0*exposure * vec3(1);
    else if (colorMode == 4)
    {
        int id = int(pointSourceId*255);
        if (id == 1)
        {
           pointColor = vec3(1,1,0);
           markerShape = 2;
        }
        else
        {
           pointColor = vec3(1,0,1);
           markerShape = 3;
        }
    }
}

