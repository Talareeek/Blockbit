// Animated-block fragment shader.
//
// The chunk mesh is static: vertex positions and texCoords point at the
// frame-0 slice in the block atlas. Animation data is packed into the
// vertex color by Render.cpp::appendQuad, so no mesh rebuild is needed
// to advance a frame.
//
// Vertex color packing (unused by the game otherwise):
//   R = frame_count            (1  = no animation, shader short-circuits)
//   G = frame_width_texels     (frame width in atlas pixels, 0..255)
//   B = frame_duration_cs      (frame duration in centiseconds, 0..255)
//   A = 255                    (unused, kept full to avoid alpha surprises)
//
// u_time       : seconds since the client started
// u_atlas_size : BlockAtlas texture dimensions in pixels

uniform sampler2D texture;
uniform float u_time;
uniform vec2 u_atlas_size;

void main()
{
    vec2 uv = gl_TexCoord[0].xy;

    float frame_count = floor(gl_Color.r * 255.0 + 0.5);

    if (frame_count > 1.5)
    {
        float frame_width_texels = floor(gl_Color.g * 255.0 + 0.5);
        float frame_duration_cs  = floor(gl_Color.b * 255.0 + 0.5);
        float frame_duration_s   = max(frame_duration_cs, 1.0) * 0.01;

        float frame = mod(floor(u_time / frame_duration_s), frame_count);

        uv.x += frame * (frame_width_texels / u_atlas_size.x);
    }

    gl_FragColor = texture2D(texture, uv);
}
