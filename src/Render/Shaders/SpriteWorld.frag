#version 450
// World layer, into the G-buffer: colour and normal + emission, both premultiplied by alpha so that
// overlapping sprites blend like the colour does. The light pass reads them back.
layout(set = 2, binding = 0) uniform sampler2D Image;
layout(set = 2, binding = 1) uniform sampler2D NormalMap; // xy: normal, a: emission

layout(location = 0) in vec2 InUv;
layout(location = 1) in vec4 InColor;
layout(location = 2) in float InFlash;
layout(location = 3) in vec2 InTurn;
layout(location = 0) out vec4 OutAlbedo;
layout(location = 1) out vec4 OutNormal;

void main() {
    vec4 texel = texture(Image, InUv);
    float alpha = texel.a * InColor.a;
    if (alpha < 0.004) discard;
    vec3 color = mix(texel.rgb * InColor.rgb, vec3(1.0), InFlash);
    vec4 packed = texture(NormalMap, InUv);
    vec2 n = packed.xy * 2.0 - 1.0;
    if (InTurn.y > 0.5) n.x = -n.x;
    float c = cos(InTurn.x), s = sin(InTurn.x);
    n = vec2(n.x * c - n.y * s, n.x * s + n.y * c);
    float emission = max(packed.a, InFlash);
    OutAlbedo = vec4(color * alpha, alpha);
    OutNormal = vec4(vec3(n * 0.5 + 0.5, emission) * alpha, alpha);
}
