#version 450
// A model's vertices into the world and onto the screen. SDL_GPU: vertex uniforms live in set 1.
layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec2 InUv;
layout(location = 3) in vec4 InColor;

layout(set = 1, binding = 0) uniform Object {
    mat4 ViewProjection;
    mat4 World;
    mat4 NormalMatrix; // the inverse transpose of World (its 3x3 part)
    vec4 Tint;         // the material's colour times the model's
    vec4 Options;      // linear colour, -, -, -
};

layout(location = 0) out vec3 OutWorld;
layout(location = 1) out vec3 OutNormal;
layout(location = 2) out vec2 OutUv;
layout(location = 3) out vec4 OutColor;

vec3 SrgbToLinear(vec3 c) { return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c)); }

void main() {
    vec4 world = World * vec4(InPosition, 1.0);
    gl_Position = ViewProjection * world;
    OutWorld = world.xyz;
    OutNormal = mat3(NormalMatrix) * InNormal;
    OutUv = InUv;
    vec4 color = InColor * Tint;
    OutColor = Options.x > 0.5 ? vec4(SrgbToLinear(color.rgb), color.a) : color;
}
