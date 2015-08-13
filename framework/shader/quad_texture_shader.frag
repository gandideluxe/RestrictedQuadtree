#version 140
#extension GL_ARB_shading_language_420pack : require
#extension GL_ARB_explicit_attrib_location : require

uniform sampler2D Texture;
in vec2 vtexturePos;
layout(location = 0) out vec4 FragColor;

void main()
{
	vec4 color = texture( Texture, vec2(vtexturePos.x, 1.0 - vtexturePos.y));

	if(color.r <= 0.0001)
	{
		color.b = 0.9;
	}
	else{
		color.r += 1.0;
		color.r = pow(color.r, 20.0);
		color.r -= 1.0;
		color.a = 1.0; 
	}
	FragColor = color;
}