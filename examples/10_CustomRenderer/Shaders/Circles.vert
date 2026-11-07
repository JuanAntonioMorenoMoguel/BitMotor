#version 450
#extension GL_GOOGLE_include_directive : require
// One quad per instance (6 vertices, no vertex buffer), placed from the storage buffer of circles.
#define GLOBALS_SET 1
#include "Globals.glsl"

struct Circle {
    vec2 Position; // screen pixels
    float Radius;
    float Unused;
    vec4 Tint;
};
layout(std430, set = 0, binding = 0) readonly buffer Circles { Circle Items[]; };

layout(location = 0) out vec2 OutLocal; // -1..1 across the quad
layout(location = 1) out vec4 OutTint;

const vec2 Corners[6] = vec2[](vec2(-1, -1), vec2(1, -1), vec2(1, 1), vec2(-1, -1), vec2(1, 1), vec2(-1, 1));

void main() {
    Circle circle = Items[gl_InstanceIndex];
    vec2 local = Corners[gl_VertexIndex];
    vec2 pixel = circle.Position + local * (circle.Radius + 1.0); // a pixel more, for the soft edge
    gl_Position = vec4(pixel.x * Screen.z * 2.0 - 1.0, 1.0 - pixel.y * Screen.w * 2.0, 0.0, 1.0);
    OutLocal = local * (circle.Radius + 1.0) / circle.Radius;
    OutTint = circle.Tint;
}
