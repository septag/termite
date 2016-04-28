$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(u_texture, 0);

void main()
{
    gl_FragColor = v_color0 * texture2D(u_texture, v_texcoord0).aaaa;
}