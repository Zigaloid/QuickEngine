$input a_position, a_normal, a_texcoord0
$output v_normal, v_view, v_texcoord0

#include "bgfx_shader.sh"

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));

    // use first model matrix (u_model is an array)
    v_normal = mul(u_model[0], vec4(a_normal, 0.0)).xyz;

    // view-space position (keep if used)
    v_view = mul(u_modelView, vec4(a_position, 1.0)).xyz;

    // pass through texcoord
    v_texcoord0 = a_texcoord0;
}