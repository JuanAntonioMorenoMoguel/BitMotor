#version 450
#extension GL_GOOGLE_include_directive : require
// The HDR trails onto the screen: exposure (Gpu::SetGlobal 0), a soft tone curve and a vignette.
#define GLOBALS_SET 3
#include "Globals.glsl"

layout(set = 2, binding = 0) uniform sampler2D Trails;
layout(location = 0) in vec2 InUv;
layout(location = 0) out vec4 Out;

void main() {
    vec3 hdr = texture(Trails, InUv).rgb * Values[0].x;
    vec3 color = hdr / (1.0 + hdr); // Reinhard: never above 1
    vec2 centered = InUv * 2.0 - 1.0;
    color *= 1.0 - 0.35 * dot(centered, centered);
    Out = vec4(color, 1.0);
}
