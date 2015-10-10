#version 150
// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 modelViewProjectionMatrix;

//------------------------------------------------------------------------------
#if defined(VERTEX_SHADER)

uniform float pointRadius = 0.1;   //# uiname=Point Radius (m); min=0.001; max=10
uniform float trimRadius = 1000000;//# uiname=Trim Radius; min=1; max=1000000
uniform float exposure = 1.0;      //# uiname=Exposure; min=0.01; max=10000
uniform float contrast = 1.0;      //# uiname=Contrast; min=0.01; max=10000
uniform int colorMode = 0;         //# uiname=Colour Mode; enum=Intensity|Colour|Coverage|Number Of Returns|Moreton Index|Las Classification|Classified|File Number|Height Above Ground
uniform int selectedTreeLevel = 0;         //# uiname=Tree Level
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
in float coverage;
in int numberOfReturns;
in int pointSourceId;
in int classification;
in int chunkIdx;
in int treeLevel;
in int leafIdx;
in float heightAboveGround;

flat out float modifiedPointRadius;
flat out float pointScreenSize;
flat out vec3 pointColor;
flat out int markerShape;
flat out float scaledCoverage;

float tonemap(float x, float exposure, float contrast)
{
    float Y = pow(exposure*x, contrast);
    Y = Y / (1.0 + Y);
    return Y;
}

vec3 jet_colormap(float x)
{
    if (x < 0.125)
        return vec3(0, 0, 0.5 + 4*x);
    if (x < 0.375)
        return vec3(0, 4*(x-0.125), 1);
    if (x < 0.625)
        return vec3(4*(x-0.375), 1, 1 - 4*(x-0.375));
    if (x < 0.875)
        return vec3(1, 1 - 4*(x-0.625), 0);
    return vec3(1 - 4*(x-0.875), 0, 0);
}

void main()
{
    vec4 p = modelViewProjectionMatrix * vec4(position,1.0);
    float r = length(position.xy - cursorPos.xy);
    float trimFalloffLen = min(5, trimRadius/2);
    float trimScale = min(1, (trimRadius - r)/trimFalloffLen);
    scaledCoverage = 17*coverage;
    modifiedPointRadius = pointRadius * trimScale * pointSizeLodMultiplier * (1<<8)/(1<<treeLevel);
    pointScreenSize = clamp(2*pointPixelScale*modifiedPointRadius / p.w, minPointSize, maxPointSize);
    markerShape = 0;
    // Compute vertex color
    if (colorMode == 0)
        pointColor = tonemap(intensity/400.0, exposure, contrast) * vec3(1);
    else if (colorMode == 1)
        pointColor = contrast*(exposure*color - vec3(0.5)) + vec3(0.5);
    else if (colorMode == 2)
        pointColor = 0.2*coverage*exposure * vec3(1);
    else if (colorMode == 3)
        pointColor = 0.2*numberOfReturns*exposure * vec3(1);
    else if (colorMode == 4)
    {
        markerShape = chunkIdx % 5;
        pointColor = jet_colormap(leafIdx /15999.0/exposure);
        markerShape = leafIdx % 5;
       // pointColor = vec3((chunkIdx % 100)/99.0)
         //          + vec3((leafIdx % 10000)/9999.0, (leafIdx % 1000)/999.0, 0);
    }
    else if (colorMode == 5)
    {
        // Colour according to some common classifications defined in the LAS spec
        pointColor = vec3(exposure*classification);
        if (classification == 2)      pointColor = vec3(0.33, 0.18, 0.0); // ground
        else if (classification == 3) pointColor = vec3(0.25, 0.49, 0.0); // low vegetation
        else if (classification == 4) pointColor = vec3(0.36, 0.7,  0.0); // medium vegetation
        else if (classification == 5) pointColor = vec3(0.52, 1.0,  0.0); // high vegetation
        else if (classification == 6) pointColor = vec3(0.8,  0.0,  0.0); // building
        else if (classification == 9) pointColor = vec3(0.0,  0.0,  0.8); // water
    }
    else if (colorMode == 6)
    {
        // Use intensity, but shade all points which have been classified green
        pointColor = tonemap(intensity/400.0, exposure, contrast) * vec3(1);
        if (classification != 0)
            pointColor = 0.5*pointColor + vec3(0,0.75,0);
    }
    else if (colorMode == 7)
    {
        // Set point colour and marker shape cyclically based on file number to
        // give a unique combinations for 5*7 files.
        markerShape = fileNumber % 5;
        vec3 cols[] = vec3[](vec3(1,1,1), vec3(1,0,0), vec3(0,1,0), vec3(0,0,1),
                             vec3(1,1,0), vec3(1,0,1), vec3(0,1,1));
        pointColor = cols[fileNumber % 7];
    }
    else if (colorMode == 8)
    {
        // Color based on height above ground
        pointColor = 0.8*jet_colormap(tonemap(0.16*heightAboveGround, exposure, 3.8*contrast));
    }
    if (selectedTreeLevel != treeLevel)
        pointScreenSize = 0;
    // Ensure zero size points are discarded.  The actual minimum point size is
    // hardware and driver dependent, so set the markerShape to discarded for
    // good measure.
    if (pointScreenSize <= 0)
    {
        pointScreenSize = 0;
        markerShape = -1;
    }
    else if (pointScreenSize < 1)
    {
        // Clamp to minimum size of 1 to avoid aliasing with some drivers
        pointScreenSize = 1;
    }
    gl_PointSize = pointScreenSize;
    gl_Position = p;
}


//------------------------------------------------------------------------------
#elif defined(FRAGMENT_SHADER)

uniform float markerWidth = 0.3;

flat in float modifiedPointRadius;
flat in float pointScreenSize;
flat in float scaledCoverage;
flat in vec3 pointColor;
flat in int markerShape;

out vec4 fragColor;

// Limit at which the point is rendered as a small square for antialiasing
// rather than using a specific marker shape
const float pointScreenSizeLimit = 2;
const float sqrt2 = 1.414213562;

void main()
{
    // Use simple ordered dithering to simulate partial coverage by a voxel
    float stippleThresholds[] = float[](
        1, 9, 3, 11,
        13, 5, 15, 7,
        4, 12, 2, 10,
        16, 8, 14, 6
    );
    if (stippleThresholds[int(gl_FragCoord.x) % 4 + 4*(int(gl_FragCoord.y) % 4)] > scaledCoverage)
        discard;
    if (markerShape < 0) // markerShape == -1: discarded.
        discard;
    // (markerShape == 0: Square shape)
#   ifndef BROKEN_GL_FRAG_COORD
    gl_FragDepth = gl_FragCoord.z;
#   endif
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
#           ifndef BROKEN_GL_FRAG_COORD
            gl_FragDepth += projectionMatrix[3][2] * gl_FragCoord.w*gl_FragCoord.w
                            // TODO: Why is the factor of 0.5 required here?
                            * 0.5*modifiedPointRadius*sqrt(1-r*r);
#           endif
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

