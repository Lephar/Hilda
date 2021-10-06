#version 460 core

layout(binding = 0) uniform Transform {
	mat4 transform;
};

layout(location = 0) in vec3 inputPosition;
layout(location = 1) in vec3 inputNormal;
layout(location = 2) in vec2 inputTexture;

layout(location = 0) out vec3 outputPosition;
layout(location = 1) out vec3 outputNormal;
layout(location = 2) out vec2 outputTexture;

vec4 distort(vec4 p)
{
	vec2 v = p.xy / p.w;

	float theta  = atan(v.y,v.x);
	float radius = pow(length(v), 0.75);

	v.x = radius * cos(theta);
	v.y = radius * sin(theta);
	p.xy = v.xy * p.w;

	return p;
}

void main()
{
	outputPosition = inputPosition;
	outputNormal = inputNormal;
	outputTexture = inputTexture;
	
	gl_Position = transform * vec4(outputPosition, 1.0f);
	//gl_Position = distort(transform * vec4(outputPosition, 1.0f));
}
