#version 450
// One triangle that covers the whole target: draw 3 vertices, no vertex buffer. Uv: 0,0 at the top left.
layout(location = 0) out vec2 OutUv;

void main() {
    vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
    OutUv = vec2(corner.x, 1.0 - corner.y);
}
