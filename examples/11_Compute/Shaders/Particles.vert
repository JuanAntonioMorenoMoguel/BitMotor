#version 450
#extension GL_GOOGLE_include_directive : require
// A small quad per particle (6 vertices, no vertex buffer), read from the buffer the compute shader wrote.
#define GLOBALS_SET 1
#include "Globals.glsl"
#include "Particle.glsl"

layout(std430, set = 0, binding = 0) readonly buffer Particles { Particle Items[]; };

layout(location = 0) out vec2 OutLocal;
layout(location = 1) out vec4 OutTint;

const vec2 Corners[6] = vec2[](vec2(-1, -1), vec2(1, -1), vec2(1, 1), vec2(-1, -1), vec2(1, 1), vec2(-1, 1));

void main() {
    Particle p = Items[gl_InstanceIndex];
    vec2 local = Corners[gl_VertexIndex];
    float speed = length(p.Velocity);
    vec2 pixel = p.Position + local * 1.5;
    gl_Position = vec4(pixel.x * Screen.z * 2.0 - 1.0, 1.0 - pixel.y * Screen.w * 2.0, 0.0, 1.0);
    OutLocal = local;
    OutTint = vec4(p.Tint.rgb * (0.4 + min(speed / 120.0, 2.0)), p.Tint.a);
}
