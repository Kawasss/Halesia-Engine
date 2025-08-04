#version 460
layout (location = 0) out vec4 gridColor;

layout (location = 0) in vec3 coor;
layout (location = 1) in vec3 coor3D;
layout (location = 2) in vec3 oPlayerPos;

vec4 grid(vec3 fragPos3D, float scale, bool drawAxis) 
{
    vec2 coord = fragPos3D.xz * scale;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float minimumz = min(derivative.y, 1);
    float minimumx = min(derivative.x, 1);
    vec4 color = vec4(0.08, 0.08, 0.08, 1.0 - min(line, 1.0));
    
    bool isBlue = fragPos3D.x > -minimumx && fragPos3D.x <  minimumx;
    bool isRed  = fragPos3D.z > -minimumz && fragPos3D.z < minimumz;

    if (isBlue || isRed)
        color.xyz = vec3(0);

    if (isBlue)
        color.b = 1.0;

    if (isRed)
        color.r = 1.0;

    return color;
}

void main()
{
    const float maxFogDist = 50;

    vec3 camPosFog = vec3(oPlayerPos.x, 0.0, oPlayerPos.z);

    float dist = distance(camPosFog, coor3D);
    float opacity = (maxFogDist - dist) / maxFogDist;

    gridColor = grid(coor3D, 1, true) + grid(coor3D, 1, true);
    gridColor.a *= opacity;
}