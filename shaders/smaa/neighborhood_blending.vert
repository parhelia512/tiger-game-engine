#version 460 core

#define SMAA_INCLUDE_VS 1
#define SMAA_INCLUDE_PS 0

#include "smaa/common_defines.glsl"
#include "smaa.hlsl"

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;

out vec4 vOffset;
out vec2 vTexCoord;

void main() {
    gl_Position = vec4(aPosition, 1);
    vTexCoord = aTexCoord;
    SMAANeighborhoodBlendingVS(vTexCoord, vOffset);
}