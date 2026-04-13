$input v_normal, v_view, v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_texColor, 0);

uniform vec4 u_lightDir;
uniform vec4 u_lightColor;
uniform vec4 u_ambient;
uniform vec4 u_materialColor;

void main()
{
    vec3 normal = normalize(v_normal);
    float nDotL = max(dot(normal, normalize(-u_lightDir.xyz)), 0.0);
    vec3 diffuse = nDotL * u_lightColor.rgb;
    vec3 baseColor = (u_ambient.rgb + diffuse) * u_materialColor.rgb;

    vec4 texColor = texture2D(s_texColor, v_texcoord0);
    vec3 finalColor = baseColor * texColor.rgb;
    float outAlpha = texColor.a * u_materialColor.a;

    gl_FragColor = vec4(finalColor, outAlpha);
}