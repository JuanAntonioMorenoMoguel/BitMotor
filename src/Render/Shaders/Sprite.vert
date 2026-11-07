#version 450
// One sprite = one instance of 6 vertices (two triangles), read from the storage buffer.
// SDL_GPU: vertex storage buffers live in set 0, vertex uniforms in set 1.
struct Sprite {
    vec4 A; // position x, y (screen pixels, the pivot), size x (negative = flipped), size y
    vec4 B; // area in the texture: x, y, w, h (texture pixels)
    vec4 C; // colour (straight alpha)
    vec4 D; // rotation (radians), flash, pivot x, pivot y (fractions of the size)
};
layout(std430, set = 0, binding = 0) readonly buffer Sprites {
    Sprite Items[];
};
layout(set = 1, binding = 0) uniform Frame {
    vec4 Screen;  // screen size xy, first instance of this batch, -
    vec4 Texture; // texture size xy, -, -
};

layout(location = 0) out vec2 OutUv;
layout(location = 1) out vec4 OutColor;
layout(location = 2) out float OutFlash;

const vec2 Corners[6] = vec2[](vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 0), vec2(1, 1), vec2(0, 1));

void main() {
    Sprite s = Items[int(Screen.z) + gl_InstanceIndex];
    vec2 corner = Corners[gl_VertexIndex];
    vec2 size = s.A.zw;
    bool flipped = size.x < 0.0;
    size.x = abs(size.x);
    vec2 pivot = s.D.zw;
    if (flipped) pivot.x = 1.0 - pivot.x;
    vec2 position;
    if (s.D.x == 0.0) {
        // Unrotated sprites snap their corner to the pixel grid: crisp pixels between two positions.
        position = floor(s.A.xy - pivot * size + 0.5) + corner * size;
    } else {
        vec2 local = (corner - pivot) * size;
        float c = cos(s.D.x), n = sin(s.D.x);
        position = s.A.xy + vec2(local.x * c - local.y * n, local.x * n + local.y * c);
    }
    // Screen pixels (y down) to clip space (y up).
    vec2 clip = position / Screen.xy * 2.0 - 1.0;
    gl_Position = vec4(clip.x, -clip.y, 0.0, 1.0);
    vec2 uvCorner = vec2(flipped ? 1.0 - corner.x : corner.x, corner.y);
    OutUv = (s.B.xy + uvCorner * s.B.zw) / Texture.xy;
    OutColor = s.C;
    OutFlash = s.D.y;
}
