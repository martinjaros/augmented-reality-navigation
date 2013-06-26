all:
	mkdir -p bin
	xxd -i src/shader.frag > src/shaders.inc
	xxd -i src/shader.vert >> src/shaders.inc
	gcc -g -Wall -I/usr/include/freetype2 -DX11BUILD src/*.c -o bin/arnav -lm -lfreetype -lGLESv2 -lEGL -lX11
	doxygen

clean:
	rm bin doc -r -f