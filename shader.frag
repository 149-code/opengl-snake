#version 330 core

in vec3 in_frag_color;
out vec4 FragColor;

void main()
{
	FragColor = vec4(in_frag_color, 1.0);
}
