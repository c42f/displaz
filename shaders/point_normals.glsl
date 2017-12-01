#version 130
// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

// This shader allows normals to be supplied with each point, and draws a small
// line segment in the direction of the normal, centred on the point.

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 modelViewProjectionMatrix;

//------------------------------------------------------------------------------
#if defined(VERTEX_SHADER)

uniform float pointSize = 10.0;    //# uiname=Point Size; min=1; max=200
uniform float minPointSize = 0;
uniform float maxPointSize = 200.0;
// Point size multiplier to keep coverage constant when doing stochastic
// simplification
uniform float pointSizeLodMultiplier = 1;
in vec3 position;
in vec3 normal;
in vec3 color;

flat out float pointScreenSize;
flat out vec3 pointColor;
flat out vec2 lineNormal;
flat out float lineNormalLen;

void main()
{
    vec4 p = modelViewProjectionMatrix * vec4(position,1.0);
    gl_Position = p;
    float wInv = 1.0/p.w;
    // Compute differential of the projection Proj(v) = (A*v).xy / (A*v).w
    // restricted to the xy plane.  dProj given here has a factor of wInv
    // removed so that the resulting lineNormalLen is correct for the point
    // coordinate system (pointScreenSize is effectively absorbing the factor
    // of wInv).
    mat3x2 dProj = mat3x2(modelViewProjectionMatrix) -
                   outerProduct(wInv*p.xy, transpose(modelViewProjectionMatrix)[3].xyz);
    // Remove aspect ratio - fragment coord system will be square.
    float aspect = projectionMatrix[1][1]/projectionMatrix[0][0];
    dProj = mat2x2(aspect, 0, 0, 1) * dProj;
    vec2 dirProj = dProj*normalize(normal);
    lineNormalLen = length(dirProj);
    lineNormal = vec2(-dirProj.y, dirProj.x) / lineNormalLen;
    pointScreenSize = clamp(20.0*pointSize * wInv * pointSizeLodMultiplier,
                            minPointSize, maxPointSize);
    gl_PointSize = pointScreenSize;
    pointColor = color;
}


//------------------------------------------------------------------------------
#elif defined(FRAGMENT_SHADER)

uniform float markerWidth = 0.3;  // # uiname=Marker Width; min=0.01; max=1

flat in float pointScreenSize;
flat in vec3 pointColor;
flat in vec2 lineNormal;
flat in float lineNormalLen;

out vec4 fragColor;

// Limit at which the point is rendered as a small square for antialiasing
// rather than using a specific marker shape
const float pointScreenSizeLimit = 2;

void main()
{
    if (pointScreenSize <= 0)
        discard;
    if (pointScreenSize > pointScreenSizeLimit)
    {
        float w = markerWidth;
        if (pointScreenSize < 2*pointScreenSizeLimit)
        {
            // smoothly turn on the markers as we get close enough to see them
            w = mix(1, w, pointScreenSize/pointScreenSizeLimit - 1);
        }
        vec2 p = 2*(gl_PointCoord - 0.5);
        p.y = -p.y;
        float r = length(p);
        const float lineRad = 1.0;
        bool inLine = r*(1-w) < max(0.5*lineNormalLen, 2/pointScreenSize) &&
                abs(dot(lineNormal,p))*(1-w)*pointScreenSize < lineRad;
        if (!inLine)
            discard;
    }
    fragColor = vec4(pointColor, 1);
}

#endif

