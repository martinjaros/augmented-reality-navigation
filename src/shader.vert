attribute vec4 coord;
uniform vec2 offset;
varying vec2 texpos;

void main(void)
{
  gl_Position = vec4(coord.xy + offset, 0, 1);
  texpos = coord.zw;
}
