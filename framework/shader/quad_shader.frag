#version 140
#extension GL_ARB_shading_language_420pack : require
#extension GL_ARB_explicit_attrib_location : require

in vec3 vColor;
layout(location = 0) out vec4 FragColor;

void main()
{
FragColor = vec4(vColor, 1.0);
}
