uniform vec4 color;
uniform vec4 mask;
uniform sampler2D tex;
varying vec2 texpos;

void main(void)
{
  gl_FragColor = texture2D(tex, texpos) * mask + color;
}
