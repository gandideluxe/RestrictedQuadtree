#version 140
#extension GL_ARB_shading_language_420pack : require
#extension GL_ARB_explicit_attrib_location : require

layout(location = 0) in vec3 position;
out vec2 vtexturePos;
uniform mat4 Projection;
uniform mat4 Modelview;

void main()
{
vtexturePos = position.xy;
vec4 Position = vec4(position, 1.0);
gl_Position = Projection * Modelview * Position;
}