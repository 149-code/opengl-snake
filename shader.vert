#version 330 core

layout (location = 0) in vec2 vertex;
uniform vec3 in_color;
uniform vec2 pos;
uniform float board_size;

out vec3 in_frag_color;

void main()
{
	gl_Position = vec4(vertex / board_size + pos / board_size, 1.0, 1.0) * 2 - 1;
	in_frag_color = in_color;
}
