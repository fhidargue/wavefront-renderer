#version 330 core

in vec2 fragTexCoord;
out vec4 fragColour;

uniform sampler2D renderTexture;

void main()
{
    fragColour = vec4(texture(renderTexture, fragTexCoord).rgb, 1.0);
}