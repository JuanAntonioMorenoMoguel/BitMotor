#version 450
// A model's pixels: texture x colour, lit by the ambient, the sun and up to 8 point lights, optionally in
// bands with ordered dithering like the 2D lighting. Into the HDR scene. SDL_GPU: samplers in set 2,
// fragment uniforms in set 3.
layout(set = 2, binding = 0) uniform sampler2D Image;
layout(set = 3, binding = 0) uniform Light {
    vec4 Sun;      // direction the light travels (xyz), -
    vec4 SunColor; // rgb x intensity, -
    vec4 Ambient;  // rgb, stepped
    vec4 Options;  // emission, unlit, linear colour, light count
    vec4 LightPosition[8]; // xyz, radius
    vec4 LightColor[8];    // rgb x intensity, -
};

layout(location = 0) in vec3 InWorld;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec2 InUv;
layout(location = 3) in vec4 InColor;
layout(location = 0) out vec4 Out;

vec3 SrgbToLinear(vec3 c) { return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c)); }

float Bayer4(vec2 p) {
    ivec2 i = ivec2(mod(p, 4.0));
    int m[16] = int[](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
    return float(m[i.y * 4 + i.x]) / 16.0;
}

void main() {
    vec4 texel = texture(Image, InUv);
    if (Options.z > 0.5) texel.rgb = SrgbToLinear(texel.rgb);
    vec4 albedo = texel * InColor;
    if (albedo.a < 0.004) discard;
    vec3 glow = albedo.rgb * Options.x * 1.5;
    if (Options.y > 0.5) {
        Out = vec4(albedo.rgb + glow, albedo.a);
        return;
    }
    vec3 n = normalize(InNormal);
    if (!gl_FrontFacing) n = -n;
    vec3 light = Ambient.rgb + SunColor.rgb * max(dot(n, -Sun.xyz), 0.0);
    int count = int(Options.w);
    for (int i = 0; i < count; ++i) {
        vec3 d = LightPosition[i].xyz - InWorld;
        float distance = length(d);
        float fade = clamp(1.0 - distance / max(LightPosition[i].w, 0.001), 0.0, 1.0);
        if (fade <= 0.0) continue;
        fade *= fade;
        light += LightColor[i].rgb * fade * (0.25 + 0.75 * max(dot(n, d / max(distance, 1e-4)), 0.0));
    }
    if (Ambient.w > 0.5) {
        float level = max(max(light.r, light.g), light.b);
        float perceived = Options.z > 0.5 ? pow(level, 1.0 / 2.2) : level;
        float banded = floor(perceived * 7.0 + Bayer4(floor(gl_FragCoord.xy)) * 0.9) / 7.0;
        if (Options.z > 0.5) banded = pow(banded, 2.2);
        light *= banded / max(level, 1e-4);
    }
    Out = vec4(albedo.rgb * light + glow, albedo.a);
}
