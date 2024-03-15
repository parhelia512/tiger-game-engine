#version 460 core

// This shader performs upsampling on a texture,
// as taken from Call Of Duty method, presented at ACM Siggraph 2014.

// Remember to add bilinear minification filter for this texture!
// Remember to use a floating-point texture format (for HDR)!
// Remember to use edge clamping for this texture!
uniform sampler2D uSrcTexture;
uniform float uFilterRadius;
uniform vec4 uViewport;

layout (location = 0) out vec3 upsample;

void main() {
    vec2 texCoord = gl_FragCoord.xy / uViewport.zw;

    // The filter kernel is applied with a radius, specified in texture
    // coordinates, so that the radius will vary across mip resolutions.
    float x = uFilterRadius;
    float y = uFilterRadius;

    // Take 9 samples around current texel:
    // a - b - c
    // d - e - f
    // g - h - i
    // === ('e' is the current texel) ===
    vec3 a = texture(uSrcTexture, vec2(texCoord.x - x, texCoord.y + y)).rgb;
    vec3 b = texture(uSrcTexture, vec2(texCoord.x,     texCoord.y + y)).rgb;
    vec3 c = texture(uSrcTexture, vec2(texCoord.x + x, texCoord.y + y)).rgb;

    vec3 d = texture(uSrcTexture, vec2(texCoord.x - x, texCoord.y)).rgb;
    vec3 e = texture(uSrcTexture, vec2(texCoord.x,     texCoord.y)).rgb;
    vec3 f = texture(uSrcTexture, vec2(texCoord.x + x, texCoord.y)).rgb;

    vec3 g = texture(uSrcTexture, vec2(texCoord.x - x, texCoord.y - y)).rgb;
    vec3 h = texture(uSrcTexture, vec2(texCoord.x,     texCoord.y - y)).rgb;
    vec3 i = texture(uSrcTexture, vec2(texCoord.x + x, texCoord.y - y)).rgb;

    // Apply weighted distribution, by using a 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    upsample = e * 4.0;
    upsample += (b + d + f + h) * 2.0;
    upsample += (a + c + g + i);
    upsample *= 1.0 / 16.0;
}