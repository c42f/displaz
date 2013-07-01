#version 130



//------------------------------------------------------------------------------
#if defined(VERTEX_SHADER)

uniform float pointRadius = 0.1;   //# uiname=Point Radius (m); min=0.001; max=10
uniform float trimRadius = 1000000;//# uiname=Trim Radius; min=1; max=1000000
uniform int selector = 0;          //# uiname=File Selector; enum=All Files|$FILE_LIST
uniform float exposure = 1.0;      //# uiname=Exposure; min=0.01; max=10000
uniform float contrast = 1.0;      //# uiname=Contrast; min=0.01; max=10000
uniform int colorMode = 0;         //# uiname=Colour Mode; enum=Intensity|Colour|Return Number|Number Of Returns|Point Source|Classification|File Number
uniform float minPointSize = 0;
uniform float maxPointSize = 400.0;
// Point size multiplier to keep coverage constant when doing stochastic
// simplification
uniform float pointSizeLodMultiplier = 1;
// Point size multiplier to get from a width in projected coordinates to the
// number of pixels across as required for gl_PointSize
uniform float pointPixelScale = 0;
uniform vec3 cursorPos = vec3(0);
uniform int fileNumber = 0;
in float intensity;
in vec3 position;
in vec3 color;
// FIXME: Should avoid turning these two into floats!
in float returnNumber;
in float numberOfReturns;
in float pointSourceId;
in float classification;

flat out float modifiedPointRadius;
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
    vec4 p = gl_ModelViewProjectionMatrix * vec4(position,1.0);
    float r = length(position.xy - cursorPos.xy);
    float trimFalloffLen = min(5, trimRadius/2);
    float trimScale = min(1, (trimRadius - r)/trimFalloffLen);
    modifiedPointRadius = pointRadius * trimScale * pointSizeLodMultiplier;
    pointScreenSize = clamp(2*pointPixelScale*modifiedPointRadius / p.w, minPointSize, maxPointSize);
    if (selector > 0 && selector != fileNumber)
        pointScreenSize = 0;
    markerShape = 1;
    // Compute vertex color
    if (colorMode == 0)
        pointColor = tonemap(intensity/400.0, exposure, contrast) * vec3(1);
    else if (colorMode == 1)
        pointColor = contrast*(exposure*color - vec3(0.5)) + vec3(0.5);
    else if (colorMode == 2)
        pointColor = returnNumber*51.0*exposure * vec3(1);
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
    else if (colorMode == 5)
    {
        // Default coloring: greyscale
        pointColor = vec3(exposure*classification);
        // Some special colors for standard ASPRS classification numbers
        int cl = int(classification*255);
        if (cl == 2)      pointColor = vec3(0.33, 0.18, 0.0); // ground
        else if (cl == 3) pointColor = vec3(0.25, 0.49, 0.0); // low vegetation
        else if (cl == 4) pointColor = vec3(0.36, 0.7,  0.0); // medium vegetation
        else if (cl == 5) pointColor = vec3(0.52, 1.0,  0.0); // high vegetation
        else if (cl == 6) pointColor = vec3(0.8,  0.0,  0.0); // building
        else if (cl == 9) pointColor = vec3(0.0,  0.0,  0.8); // water
    }
    else if (colorMode == 6)
    {
        // Set point colour and marker shape cyclically based on file number to
        // give a unique combinations for 5*7 files.
        markerShape = fileNumber % 5;
        vec3 cols[] = vec3[](vec3(1,1,1), vec3(1,0,0), vec3(0,1,0), vec3(0,0,1),
                             vec3(1,1,0), vec3(1,0,1), vec3(0,1,1));
        pointColor = cols[fileNumber % 7];
    }
    // Ensure zero size points are discarded.  The actual minimum point size is
    // hardware and driver dependent, so set the markerShape to discarded for
    // good measure.
    if (pointScreenSize <= 0)
    {
        pointScreenSize = 0;
        markerShape = -1;
    }
    gl_PointSize = pointScreenSize;
    gl_Position = p;
}


//------------------------------------------------------------------------------
#elif defined(FRAGMENT_SHADER)

uniform float markerWidth = 0.3;

flat in float modifiedPointRadius;
flat in float pointScreenSize;
flat in vec3 pointColor;
flat in int markerShape;

out vec4 fragColor;

// Limit at which the point is rendered as a small square for antialiasing
// rather than using a specific marker shape
const float pointScreenSizeLimit = 2;
const float sqrt2 = 1.414213562;

void main()
{
    if (markerShape < 0) // markerShape == -1: discarded.
        discard;
    // (markerShape == 0: Square shape)
    gl_FragDepth = gl_FragCoord.z;
    if (markerShape > 0 && pointScreenSize > pointScreenSizeLimit)
    {
        float w = markerWidth;
        if (pointScreenSize < 2*pointScreenSizeLimit)
        {
            // smoothly turn on the markers as we get close enough to see them
            w = mix(1, w, pointScreenSize/pointScreenSizeLimit - 1);
        }
        vec2 p = 2*(gl_PointCoord - 0.5);
        if (markerShape == 1) // shape: .
        {
            float r = length(p);
            if (r > 1)
                discard;
            gl_FragDepth += gl_ProjectionMatrix[3][2] * gl_FragCoord.w*gl_FragCoord.w
                            // TODO: Why is the factor of 0.5 required here?
                            * 0.5*modifiedPointRadius*sqrt(1-r*r);
        }
        else if (markerShape == 2) // shape: o
        {
            float r = length(p);
            if (r > 1 || r < 1 - w)
                discard;
        }
        else if (markerShape == 3) // shape: x
        {
            w *= 0.5*sqrt2;
            if (abs(p.x + p.y) > w && abs(p.x - p.y) > w)
                discard;
        }
        else if (markerShape == 4) // shape: +
        {
            w *= 0.5;
            if (abs(p.x) > w && abs(p.y) > w)
                discard;
        }
    }
    fragColor = vec4(pointColor, 1);
}

#endif

