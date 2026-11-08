#version 450
// The texture times the sprite's colour, flashed towards white. SDL_GPU: fragment samplers live in set 2.
layout(set = 2, binding = 0) uniform sampler2D Image;

layout(location = 0) in vec2 InUv;
layout(location = 1) in vec4 InColor;
layout(location = 2) in float InFlash;
layout(location = 4) flat in float InLinear;
layout(location = 0) out vec4 Out;

vec3 SrgbToLinear(vec3 c) { return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c)); }

void main() {
    vec4 texel = texture(Image, InUv);
    if (InLinear > 0.5) texel.rgb = SrgbToLinear(texel.rgb);
    float alpha = texel.a * InColor.a;
    if (alpha < 0.004) discard;
    Out = vec4(mix(texel.rgb * InColor.rgb, vec3(1.0), InFlash), alpha);
}
