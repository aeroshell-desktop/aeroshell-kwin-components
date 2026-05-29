#version 140

uniform sampler2D sampler;

in vec2 texcoord0;

out vec4 fragColor;

void main()
{
    fragColor = texture2D(sampler, texcoord0);
}
