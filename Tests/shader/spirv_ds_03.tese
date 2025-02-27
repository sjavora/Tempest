#version 310 es
#extension GL_EXT_tessellation_shader : require

layout(cw, triangles, fractional_even_spacing) in;

void main()
{
  gl_Position = vec4(gl_TessCoord.x * gl_TessLevelInner[0] * gl_TessLevelOuter[0],
                     gl_TessCoord.y * gl_TessLevelInner[0] * gl_TessLevelOuter[1],
                     gl_TessCoord.z * gl_TessLevelInner[0] * gl_TessLevelOuter[2],
                     1);
}

