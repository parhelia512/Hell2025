#version 410

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texCoords;

out vec2 TexCoords;
out vec2 texCoordOffset;
out vec2 offsetTL;
out vec2 offsetTR;
out vec2 offsetBL;
out vec2 offsetBR;

uniform float u_viewportWidth;
uniform float u_viewportHeight;
uniform float u_gBufferWidth;
uniform float u_gBufferHeight;

float MapRange(float value, float oldMin, float oldMax, float newMin, float newMax) {
    return newMin + (value - oldMin) * (newMax - newMin) / (oldMax - oldMin);
}

void main() {
    vec2 viewportRatio = vec2(u_viewportWidth, u_viewportHeight) / vec2(u_gBufferWidth, u_gBufferHeight);
    TexCoords = texCoords;
    TexCoords.x = MapRange(TexCoords.x, 0.0, 1.0, 0.0, viewportRatio.x);
    TexCoords.y = MapRange(TexCoords.y, 0.0, 1.0, 0.0, viewportRatio.y);
    
    // Compute Fxaa texture offsets here, rather than fragment shader
    texCoordOffset = 1.0f / vec2(u_gBufferWidth, u_gBufferHeight);
    offsetTL = TexCoords + vec2(-1.0, -1.0) * texCoordOffset;
    offsetTR = TexCoords + (vec2(1.0, -1.0) * texCoordOffset);
    offsetBL = TexCoords + vec2(-1.0,  1.0) * texCoordOffset;
    offsetBR = TexCoords + vec2( 1.0,  1.0) * texCoordOffset;
    
    gl_Position = vec4(position, 1.0);
}
