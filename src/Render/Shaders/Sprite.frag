#version 450
// The texture times the sprite's colour, flashed towards white. SDL_GPU: fragment samplers live in set 2.
layout(set = 2, binding = 0) uniform sampler2D Image;

layout(location = 0) in vec2 InUv;
layout(location = 1) in vec4 InColor;
layout(location = 2) in float InFlash;
layout(location = 0) out vec4 Out;

void main() {
    vec4 texel = texture(Image, InUv);
    float alpha = texel.a * InColor.a;
    if (alpha < 0.004) discard;
    Out = vec4(mix(texel.rgb * InColor.rgb, vec3(1.0), InFlash), alpha);
}
