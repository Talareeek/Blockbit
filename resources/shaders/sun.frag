uniform vec2 resolution;
uniform vec2 sunCenter;
uniform float sunSize;
uniform vec4 sunColor;
uniform float glowSize;

void main()
{
    vec2 pixel = gl_FragCoord.xy;
    vec2 delta = pixel - sunCenter;
    vec2 d = abs(delta);

    float coreDist = max(d.x, d.y);
    float coreMask = 1.0 - smoothstep(sunSize - 1.5, sunSize + 1.5, coreDist);

    float glowDist = length(max(d - sunSize, 0.0));
    float glowT = clamp(glowDist / max(glowSize, 0.001), 0.0, 1.0);
    float glowMask = pow(1.0 - glowT, 2.5);

    if (coreMask <= 0.0 && glowMask <= 0.001)
        discard;

    vec3 hotCore = mix(sunColor.rgb, vec3(1.0), 0.35);
    vec3 rgb = mix(sunColor.rgb, hotCore, coreMask * 0.6);

    float alpha = max(coreMask, glowMask * sunColor.a);

    gl_FragColor = vec4(rgb, alpha);
}