SOURCES = $(wildcard src/*.c)
CFLAGS = -g -Wall
INCLUDES = -I/usr/include/freetype2
LIBS = -lm -lfreetype -lGLESv2 -lEGL

ifeq ($(X11BUILD),1)
	CFLAGS += -DX11BUILD
	LIBS += -lX11
endif

ifdef TRACE_LEVEL
	CFLAGS += -DTRACE_LEVEL=$(TRACE_LEVEL)
else
	CFLAGS += -DTRACE_LEVEL=2
endif

.PHONY: all clean doc

all: bin/arnav

clean:
	rm -r -f obj bin doc

doc:
	doxygen

bin/arnav: $(SOURCES:src/%.c=obj/%.o) |bin
	gcc -o $@ $^ $(LIBS)

obj/%.o: src/%.c |obj
	gcc $(CFLAGS) $(INCLUDES) -MMD -MF $(@:.o=.d) -c -o $@ $<

obj/graphics.o: src/shaders.inc
-include obj/*.d

src/shaders.inc: src/shader.frag src/shader.vert
	xxd -i src/shader.frag > src/shaders.inc
	xxd -i src/shader.vert >> src/shaders.inc

obj:
	mkdir -p obj

bin:
	mkdir -p bin
