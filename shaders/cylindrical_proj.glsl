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
uniform int colorMode = 0;         //# uiname=Colour Mode; enum=Intensity|Colour|Return Number|Number Of Returns|Point Source|Classification|File Number

uniform float hfov = 170;   //# uiname=Horizontal field of view; min=5; max=175; scaling=linear

uniform float minPointSize = 0;
uniform float maxPointSize = 400.0;
// Point size multiplier to get from a width in projected coordinates to the
// number of pixels across as required for gl_PointSize
uniform float pointPixelScale = 0;
uniform vec3 cursorPos = vec3(0);
uniform int fileNumber = 0;
in float intensity;
in vec3 position;
in vec3 color;
in int returnNumber;
in int numberOfReturns;
in int pointSourceId;
in int classification;

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
    vec4 p0 = modelViewMatrix * vec4(position,1.0);
    // Here we do our own cylindrical projection inside the vertex shader.
    // This is the right projection if you're standing at the centre of a
    // circularly curved screen.
    //
    // Notes:
    //
    // * Projected x should be an angle in the camera's xz plane
    //
    // * Projected y should be projected by dividing by the xz distance
    //
    // * Projected z is only necessary for the z-buffer, and we compute it the
    //   same way as usual for simplicity.
    //
    // OpenGL will automatically divide by gl_Position.w to do what it assumes
    // is a normal perspective projection.  For consistency with depth
    // calculations in the fragment shader, we'd like to avoid completely
    // subverting the w component, so we set it equal to xzlen (with sign(p0.z)
    // to ensure proper culling behind the camera).  This means we need to
    // factor it into the x and z components so OpenGL can remove it
    // again in the unwanted perspective divide.
    //
    float hfovScale = hfov*0.008726646259971648;
    float vfov = hfovScale * projectionMatrix[0][0] / projectionMatrix[1][1];
    float xzlen = length(p0.xz);
    float theta = atan(-p0.x/p0.z);
    float projZ = (projectionMatrix[2][2]*p0.z + projectionMatrix[2][3]) /
                  (projectionMatrix[3][2]*p0.z);
    vec4 p = vec4(
        xzlen * theta / hfovScale,
        p0.y / atan(vfov),
        xzlen * projZ,
        xzlen * -sign(p0.z)
    );
    // Standard stuff from default shader.  TODO: Really need to factor this code somehow
    float r = length(position.xy - cursorPos.xy);
    float trimFalloffLen = min(5, trimRadius/2);
    float trimScale = min(1, (trimRadius - r)/trimFalloffLen);
    modifiedPointRadius = pointRadius * trimScale;
    pointScreenSize = clamp(2*pointPixelScale*modifiedPointRadius / xzlen, minPointSize, maxPointSize);
    markerShape = 1;
    // Compute vertex color
    if (colorMode == 0)
        pointColor = tonemap(intensity/400.0, exposure, contrast) * vec3(1);
    else if (colorMode == 1)
        pointColor = contrast*(exposure*color - vec3(0.5)) + vec3(0.5);
    else if (colorMode == 2)
        pointColor = 0.2*returnNumber*exposure * vec3(1);
    else if (colorMode == 3)
        pointColor = 0.2*numberOfReturns*exposure * vec3(1);
    else if (colorMode == 4)
    {
        markerShape = (pointSourceId+1) % 5;
        vec3 cols[] = vec3[](vec3(1,1,1), vec3(1,0,0), vec3(0,1,0), vec3(0,0,1),
                             vec3(1,1,0), vec3(1,0,1), vec3(0,1,1));
        pointColor = cols[(pointSourceId+3) % 7];
    }
    else if (colorMode == 5)
    {
        // Default coloring: greyscale
        pointColor = vec3(exposure*classification);
        // Some special colors for standard ASPRS classification numbers
        if (classification == 2)      pointColor = vec3(0.33, 0.18, 0.0); // ground
        else if (classification == 3) pointColor = vec3(0.25, 0.49, 0.0); // low vegetation
        else if (classification == 4) pointColor = vec3(0.36, 0.7,  0.0); // medium vegetation
        else if (classification == 5) pointColor = vec3(0.52, 1.0,  0.0); // high vegetation
        else if (classification == 6) pointColor = vec3(0.8,  0.0,  0.0); // building
        else if (classification == 9) pointColor = vec3(0.0,  0.0,  0.8); // water
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

