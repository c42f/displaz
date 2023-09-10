#version 150
// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 modelViewProjectionMatrix;

//------------------------------------------------------------------------------
#if defined(VERTEX_SHADER)

uniform float pointRadius        = 0.02;    //# uiname=Point Radius; min=0.001; max=10
uniform float trimRadius         = 1000000; //# uiname=Trim Radius; min=1; max=1000000
uniform int colorMode            = 0;       //# uiname=Colour Mode; enum=Intensity|Colour|Distance
uniform float intensityReference = 40.00;   //# uiname=Intensity Reference; min=0.001; max=100000
uniform float intensityContrast  =  1.00;   //# uiname=Intensity Contrast; min=0.001; max=10000
uniform float colorExposure      =  1.00;   //# uiname=Colour Exposure; min=0.001; max=10000
uniform float colorContrast      =  1.00;   //# uiname=Colour Contrast; min=0.001; max=10000
uniform float distanceReference  =  0.05;   //# uiname=Distance Reference; min=0.001; max=500
uniform float distanceContrast   =  2.00;   //# uiname=Distance Contrast; min=0.001; max=10000
uniform float minPointSize = 0;
uniform float maxPointSize = 400.0;
// Point size multiplier to get from a width in projected coordinates to the
// number of pixels across as required for gl_PointSize
uniform float pointPixelScale = 0;
uniform vec3 cursorPos = vec3(0);

in float intensity;
in vec3 position;
in vec3 color;
in float distance;

flat out float modifiedPointRadius;
flat out float pointScreenSize;
flat out vec3 pointColor;
flat out int markerShape;
flat out float distanceF;

float tonemap(float x, float reference, float contrast)
{
    float Y = pow(x/reference, contrast);
    Y = Y / (1.0 + Y);
    return Y;
}

void main()
{
    distanceF = distance;

    vec4 p = modelViewProjectionMatrix * vec4(position,1.0);
    float r = length(position.xy - cursorPos.xy);
    float trimFalloffLen = min(5, trimRadius/2);
    float trimScale = min(1, (trimRadius - r)/trimFalloffLen);
    modifiedPointRadius = pointRadius * trimScale;
    pointScreenSize = clamp(2*pointPixelScale*modifiedPointRadius / p.w, minPointSize, maxPointSize);
    markerShape = 1;

    // Intensity
    if (colorMode == 0)
        pointColor = tonemap(intensity, intensityReference, intensityContrast) * vec3(1);

    // Color
    else if (colorMode == 1)
        pointColor = colorContrast*(colorExposure*color - vec3(0.5)) + vec3(0.5);

    // Distance
    else if (colorMode == 2)
    {
        if (distance > 0)
        {
            float v = tonemap(distance, distanceReference, distanceContrast) * 3.0;
            if (v < 1)
                pointColor = mix(vec3(0.6,  0.6,  0.6), vec3(1.0,  1.0,  0.0), v);
            else
                if (v < 2)
                    pointColor = mix(vec3(1.0,  1.0,  0.0), vec3(1.0,  0.5,  0.0), v - 1.0);
                else
                    pointColor = mix(vec3(1.0,  0.5,  0.0), vec3(1.0,  0.0,  0.0), v - 2.0);
        }
        else
        {
            float v = tonemap(-distance, distanceReference, distanceContrast) * 3.0;
            if (v < 1)
                pointColor = mix(vec3(0.6,  0.6,  0.6), vec3(0.0,  1.0,  1.0), v);
            else
                if (v < 2)
                    pointColor = mix(vec3(0.0,  1.0,  1.0), vec3(0.5,  0.0,  1.0), v - 1.0);
                else
                    pointColor = mix(vec3(0.5,  0.0,  1.0), vec3(0.0,  0.0,  1.0), v - 2.0);
        }
    }

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
flat in vec3 pointColor;
flat in int markerShape;
flat in float distanceF;

uniform float distanceLimit = 100.0;  //# uiname=Distance Limit; min=0.000; max=100.0

out vec4 fragColor;

// Limit at which the point is rendered as a small square for antialiasing
// rather than using a specific marker shape
const float pointScreenSizeLimit = 2;

void main()
{
    if (abs(distanceF) > distanceLimit)
        discard;

    if (markerShape < 0)
        discard;

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
        float r = length(p);
        if (r > 1)
            discard;
#       ifndef BROKEN_GL_FRAG_COORD
        gl_FragDepth += projectionMatrix[3][2] * gl_FragCoord.w*gl_FragCoord.w
                        // TODO: Why is the factor of 0.5 required here?
                        * 0.5*modifiedPointRadius*sqrt(1-r*r);
#       endif
    }
    fragColor = vec4(pointColor, 1);
}

#endif

