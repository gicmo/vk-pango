#version 450

layout(binding = 1) uniform sampler2D tex;

layout(location = 1) in vec2 tex_coord;

layout(location = 0) out vec4 color;

void main() {
  color =  texture(tex, tex_coord);
}
