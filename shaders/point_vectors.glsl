#version 150
// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 modelViewProjectionMatrix;

//------------------------------------------------------------------------------
#if defined(VERTEX_SHADER)

uniform float pointSize = 10.0;    //# uiname=Point Size; min=1; max=200
uniform float trimRadius = 1000000;//# uiname=Trim Radius; min=1; max=1000000
uniform int selector = 0;          //# uiname=File Selector; enum=All Files|$FILE_LIST
uniform float exposure = 1.0;      //# uiname=Exposure; min=0.01; max=10000
uniform float contrast = 1.0;      //# uiname=Contrast; min=0.01; max=10000
uniform float minPointSize = 0;
uniform float maxPointSize = 200.0;
uniform vec3 cursorPos = vec3(0);
uniform int fileNumber = 0;
in float intensity;
in vec3 position;
in vec3 normal;
in vec3 color;

flat out float pointScreenSize;
flat out vec3 pointColor;
flat out vec2 eigNormal[3];
flat out float eigLen[3];

float tonemap(float x, float exposure, float contrast)
{
    float Y = exposure*pow(x, contrast);
    Y = Y / (1.0 + Y);
    return Y;
}

void main()
{
    vec4 p = modelViewProjectionMatrix * vec4(position,1.0);
    gl_Position = p;
    float wInv = 1.0/p.w;
    // Compute differential of the projection Proj(v) = (A*v).xy / (A*v).w
    // restricted to the xy plane.  dProj given here has a factor of wInv
    // removed so that the resulting eigLen is correct for the point coordinate
    // system (pointScreenSize is effectively absorbing the factor of wInv).
    mat3x2 dProj = mat3x2(modelViewProjectionMatrix) -
                   outerProduct(wInv*p.xy, transpose(modelViewProjectionMatrix)[3].xyz);
    // Remove aspect ratio - fragment coord system will be square.
    float aspect = projectionMatrix[1][1]/projectionMatrix[0][0];
    dProj = mat2x2(aspect, 0, 0, 1) * dProj;
    vec3 pc = normalize(normal);
    //vec3 pc = normalize(position - cursorPos);
    vec3 pc1 = normalize(cross(pc, vec3(0,0,1)));
    vec3 eigs[3] = vec3[3](pc, pc1, cross(pc, pc1));
    for (int i = 0; i < 3; ++i)
    {
        vec2 dirProj = dProj*eigs[i];
        eigLen[i] = length(dirProj);
        eigNormal[i] = vec2(-dirProj.y, dirProj.x) / eigLen[i];
    }
    float r = length(position.xy - cursorPos.xy);
    float trimFalloffLen = min(5, trimRadius/2);
    float trimScale = min(1, (trimRadius - r)/trimFalloffLen);
    pointScreenSize = clamp(20.0*pointSize * wInv * trimScale,
                            minPointSize, maxPointSize);
    if (selector > 0 && selector != fileNumber)
        pointScreenSize = 0;
    gl_PointSize = pointScreenSize;
    pointColor = color;
}


//------------------------------------------------------------------------------
#elif defined(FRAGMENT_SHADER)

uniform float markerWidth = 0.3;  // # uiname=Marker Width; min=0.01; max=1

flat in float pointScreenSize;
flat in vec3 pointColor;
flat in vec2 eigNormal[3];
flat in float eigLen[3];

out vec4 fragColor;

// Limit at which the point is rendered as a small square for antialiasing
// rather than using a specific marker shape
const float pointScreenSizeLimit = 2;
const float sqrt2 = 1.414213562;

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
        bool inE1 = r*(1-w) < max(0.5*eigLen[0], 2/pointScreenSize) &&
                abs(dot(eigNormal[0],p))*(1-w)*pointScreenSize < lineRad;
        bool inE2 = r*(1-w) < max(0.5*eigLen[1], 2/pointScreenSize) &&
                abs(dot(eigNormal[1],p))*(1-w)*pointScreenSize < lineRad;
        bool inE3 = r*(1-w) < max(0.5*eigLen[2], 2/pointScreenSize) &&
                abs(dot(eigNormal[2],p))*(1-w)*pointScreenSize < lineRad;
        if (!inE1)// && !inE2 && !inE3)
            discard;
    }
    fragColor = vec4(pointColor, 1);
}

#endif

