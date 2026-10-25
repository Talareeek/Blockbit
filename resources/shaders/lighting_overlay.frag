// Lighting overlay fragment shader.
//
// Drawn as a per-chunk quad right after the chunk mesh, in a multiply blend
// mode: dst_new = src_rgb * dst_old. So this shader has to output the LIGHT
// value that the already-drawn block color should be multiplied by.
//
// Texture layout (one per chunk, 16x256 RGBA8):
//   R = block_light (0..255, encoding 0..15 as l * 17)
//   G = sky_light   (0..255, encoding 0..15 as l * 17)
//   B = unused
//   A = 255
//
// Sampled with GL_LINEAR, giving a smooth bilinear falloff across tile
// boundaries. The sampler's native 0..1 range IS our 0..1 light factor;
// no divide needed, we already encoded light * 17 above.
//
// u_day_factor      : 0 at midnight, 1 at noon. Scales sky_light so night darkens.
// u_ambient_min     : minimum visible brightness so caves don't go pitch black.
// u_effect_strength : 0 disables the lighting effect (everything full-bright);
//                     1 applies the computed lightmap as-is. User-configurable.

uniform sampler2D texture;
uniform float u_day_factor;
uniform float u_ambient_min;
uniform float u_night_sky_floor;
uniform float u_effect_strength;

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec4 s = texture2D(texture, uv);

    float block_light = s.r;
    // Sky contribution tapers with time of day but never drops below the
    // night floor — otherwise sky-exposed surfaces look pitch black at
    // midnight even though "moonlight" should still make them readable.
    float sky_scale   = mix(u_night_sky_floor, 1.0, u_day_factor);
    float sky_light   = s.g * sky_scale;

    float combined = max(block_light, sky_light);
    float lit      = max(combined, u_ambient_min);

    // Blend between full-bright (1.0) and the computed lightmap so the user
    // can scale down the overall effect without touching data uniforms.
    float final_lit = mix(1.0, lit, clamp(u_effect_strength, 0.0, 1.0));

    gl_FragColor = vec4(final_lit, final_lit, final_lit, 1.0);
}
