#version 450

layout (location = 0) in vec2 coord;

layout (binding = 0) uniform UBO
{
	mat4 projection;
	mat4 model;
	mat4 view;
} ubo;

out gl_PerVertex
{
    vec4 gl_Position;
};


void main()
{
  gl_Position = ubo.projection * ubo.view * ubo.model * vec4(coord.xy, 1.0, 1.0);
}
