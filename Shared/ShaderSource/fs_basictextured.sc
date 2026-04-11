$input v_normal, v_viewDir

#include <bgfx_shader.sh>

uniform vec4 u_lightDir;    // Directional light direction
uniform vec4 u_lightColor;  // Light color
uniform vec4 u_ambient;     // Ambient light color
uniform vec4 u_materialColor; // Object base color

void main()
{
    // Normalize interpolated normal
    vec3 normal = normalize(v_normal);
    
    // Simple Lambertian lighting
    float nDotL = max(dot(normal, normalize(-u_lightDir.xyz)), 0.0);
    vec3 diffuse = nDotL * u_lightColor.rgb;
    
    // Final color: Ambient + Diffuse
    vec3 finalColor = (u_ambient.rgb + diffuse) * u_materialColor.rgb;
    
    gl_FragColor = vec4(finalColor, 1.0);
}