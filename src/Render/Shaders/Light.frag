#version 450
// 2D lighting from the G-buffer: ambient with a soft top-left key, plus point lights with height that
// follow the normal maps. Optionally quantised into bands with ordered dithering (pixel-art look).
// Output premultiplied, over the background. Lights and pixels are both in screen pixels.
layout(set = 2, binding = 0) uniform sampler2D Albedo;
layout(set = 2, binding = 1) uniform sampler2D Normals;
struct Light {
    vec4 PositionRadius; // x, y, height, radius
    vec4 Color;          // rgb * intensity
};
layout(std430, set = 2, binding = 2) readonly buffer Lights {
    Light Items[];
};
layout(set = 3, binding = 0) uniform Params {
    vec4 Ambient; // rgb, light count
    vec4 Options; // lighting on, stepped, linear colour, -
};
layout(location = 0) out vec4 Out;

float Bayer4(vec2 p) {
    ivec2 i = ivec2(mod(p, 4.0));
    int m[16] = int[](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
    return float(m[i.y * 4 + i.x]) / 16.0;
}

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec4 g = texelFetch(Albedo, pixel, 0);
    if (g.a < 0.004) discard; // no world here: the background stays
    vec3 albedo = g.rgb / g.a;
    if (Options.x < 0.5) {
        Out = vec4(albedo * g.a, g.a);
        return;
    }
    vec4 packed = texelFetch(Normals, pixel, 0) / g.a;
    vec2 nxy = packed.xy * 2.0 - 1.0;
    vec3 n = vec3(nxy, sqrt(max(0.0, 1.0 - dot(nxy, nxy))));
    float emission = packed.b;

    vec3 light = Ambient.rgb * (0.72 + 0.28 * max(dot(n, normalize(vec3(-0.4, -0.6, 0.7))), 0.0));
    int count = int(Ambient.a);
    for (int i = 0; i < count; ++i) {
        Light l = Items[i];
        vec3 d = vec3(l.PositionRadius.xy - gl_FragCoord.xy, l.PositionRadius.z);
        float fade = clamp(1.0 - length(d.xy) / max(l.PositionRadius.w, 0.001), 0.0, 1.0);
        if (fade <= 0.0) continue;
        fade *= fade;
        light += l.Color.rgb * fade * (0.3 + 0.7 * max(dot(n, normalize(d)), 0.0));
    }
    if (Options.y > 0.5) {
        // The bands are even steps of brightness as the eye sees it: in linear light, quantise its sRGB-like
        // encoding (otherwise the dark steps are huge and the dithering turns into noise).
        float level = max(max(light.r, light.g), light.b);
        float perceived = Options.z > 0.5 ? pow(level, 1.0 / 2.2) : level;
        float banded = floor(perceived * 7.0 + Bayer4(floor(gl_FragCoord.xy)) * 0.9) / 7.0;
        if (Options.z > 0.5) banded = pow(banded, 2.2);
        light *= banded / max(level, 1e-4);
    }
    Out = vec4((albedo * light + albedo * emission * 1.5) * g.a, g.a);
}
