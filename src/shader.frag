varying vec2 texpos;
uniform sampler2D tex;
uniform vec4 color;

void main(void)
{
  vec4 pix = texture2D(tex, texpos);
  gl_FragColor = vec4(pix.rgb + color.rgb * pix.a, pix.a * color.a);
}