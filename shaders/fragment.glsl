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
layout(set = 1, binding = 1) uniform texture2D u_diffuse_map;
layout(set = 1, binding = 2) uniform texture2D u_emission_map;

layout(location = 0) in vec3 v_norm;
layout(location = 1) in vec2 v_uv;

layout(location = 0) out vec4 color;

void main()
{
	vec3 n = normalize(v_norm);

    float t = u_time;

    vec4 diffuse_map_color = texture(sampler2D(u_diffuse_map, u_sampler), v_uv);
	const vec3 lightDir = normalize(vec3(-0.74, -1, 0.68));
    vec4 diffuse_tot = max(dot(n, -lightDir), 0.3) * diffuse_map_color * diffuse;

    vec4 emission_map_color = texture(sampler2D(u_emission_map, u_sampler), v_uv);
    vec4 emission_tot = emission_map_color * emission;

	color = diffuse_tot + emission_tot;
}
