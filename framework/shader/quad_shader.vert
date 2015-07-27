#version 140
#extension GL_ARB_shading_language_420pack : require
#extension GL_ARB_explicit_attrib_location : require

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
out vec3 vColor;
uniform mat4 Projection;
uniform mat4 Modelview;

void main()
{
vColor = color;
vec4 Position = vec4(position, 1.0);
gl_Position = Projection * Modelview * Position;
}