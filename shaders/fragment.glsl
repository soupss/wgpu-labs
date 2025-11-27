#version 450

layout(set = 0, binding = 0) uniform frame {
    mat4 u_view_projection;
    float u_time;
};
layout(set = 0, binding = 2) uniform sampler u_sampler;

layout(set = 1, binding = 0) uniform material {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    vec4 emission;
    float shininess;
    float refraction;
    float dissolve;
    uint illumination;
};
layout(set = 1, binding = 1) uniform texture2D u_diffuse;
layout(set = 1, binding = 2) uniform sampler u_sampler_diffuse;

layout(location = 0) in vec3 v_norm;
layout(location = 1) in vec2 v_uv;

layout(location = 0) out vec4 color;

void main()
{
	vec3 n = normalize(v_norm);

    float t = u_time;

    vec4 diffuse_sample = texture(sampler2D(u_diffuse, u_sampler_diffuse), v_uv);

    vec4 c = vec4(diffuse_sample.rgb * diffuse.rgb, diffuse_sample.a);

	const vec3 lightDir = normalize(vec3(-0.74, -1, 0.68));
	color = c * max(dot(n, -lightDir), 0.3);
}
