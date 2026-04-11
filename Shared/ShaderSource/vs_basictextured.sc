$input a_position, a_normal
$output v_normal, v_viewDir

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_normal = mul(u_model, vec4(a_normal, 0.0)).xyz;
    
    // View direction for specular (optional) or just to pass
    vec3 worldPos = mul(u_model, vec4(a_position, 1.0)).xyz;
    v_viewDir = u_viewTexel.xyw - worldPos; // Assuming camera pos is available
}