#version 140

uniform sampler2D texUnit;
uniform float opacity;
uniform float translate;
uniform vec2 screenResolution;
uniform vec2 windowSize;
uniform mat4 colorMatrix;
uniform vec2 windowPos;

// Glow
uniform bool useWayland;
uniform vec2 textureSize;
uniform bool scaleY;
uniform sampler2D glowTexture;
uniform bool glowEnable;
uniform float glowOpacity;
uniform float windowScale;

in vec2 uv;

out vec4 fragColor;

vec4 glowFragment()
{
    float xpos = clamp(uv.x, 0.0, 1.0);

    float t_x = uv.x;
    if(xpos > 0.5) t_x = 1 - uv.x;
    else t_x = uv.x;

    float t_y = uv.y;
    if(scaleY) t_y = uv.y * windowSize.y;
    else t_y = uv.y;
    vec2 t_uv = vec2(windowSize.x * t_x / textureSize.x, windowSize.y * (1 - t_y) / textureSize.y);

    vec4 result = texture2D(glowTexture, t_uv) * glowOpacity;
    return result;
}

void main(void)
{

    float middleLine = windowPos.x + (windowSize.x * windowScale) / 2.0;
    float middleScreenLine = screenResolution.x / 2.0;
    float dx = translate * (middleScreenLine - middleLine) / 10.0;

    float x = (gl_FragCoord.x + dx) / screenResolution.x;
    float y = (gl_FragCoord.y) / screenResolution.y;

    vec2 uv;
    if(useWayland) {
        uv = vec2(x, y);
    } else {
        uv = vec2(x, -y);
    }

    vec4 result = vec4(texture(texUnit, uv).rgba) * opacity;
    if (glowEnable) {
        result += glowFragment();
    }

    fragColor = result;
    fragColor *= colorMatrix;
}
