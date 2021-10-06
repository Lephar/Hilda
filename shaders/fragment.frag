#version 460 core

layout(binding = 1) uniform sampler2D textureSampler;

layout(location = 0) in vec3 inputPosition;
layout(location = 1) in vec3 inputNormal;
layout(location = 2) in vec2 inputTexture;

layout(location = 0) out vec4 outputColor;

void main()
{
	vec3 lightPosition = vec3(-8.0f, 8.0f, 8.0f);
	vec3 lightDirection = normalize(lightPosition - inputPosition);
	float intensity = max(dot(inputNormal, lightDirection), 0.0f);
	
	vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);
	vec3 pointLight = intensity * lightColor;
	vec3 ambientLight = vec3(0.2f, 0.2f, 0.2f);
	
	outputColor = vec4(pointLight + ambientLight, 1.0f) * vec4(inputNormal, 1.0f);
	//outputColor = vec4(pointLight + ambientLight, 1.0f) * texture(textureSampler, inputTexture);
}
