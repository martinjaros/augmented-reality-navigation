SOURCES = $(wildcard src/*.c)
CFLAGS = -Wall
INCLUDES = -I/usr/include/freetype2
LIBS = -lm -lpthread -lfreetype -lGLESv2 -lEGL

ifndef CC
	CC = gcc
endif

ifdef DEBUG
	CFLAGS += -g
else
	CFLAGS += -O2
endif

ifdef X11BUILD
	CFLAGS += -DX11BUILD -DFULLSCREEN=0
	LIBS += -lX11
endif

ifdef TRACE_LEVEL
	CFLAGS += -DTRACE_LEVEL=$(TRACE_LEVEL)
else
	CFLAGS += -DTRACE_LEVEL=2
endif

ifdef DRIVER
	CFLAGS += -DDRIVER_$(DRIVER)
else
	CFLAGS += -DDRIVER_ITG3200_AK8975_BMA150
endif

.PHONY: all clean doc

all: bin/arnav

clean:
	@rm -r -f obj bin doc

doc:
	@doxygen

bin/arnav: $(SOURCES:src/%.c=obj/%.o) |bin
	@echo LD $@
	@$(CC) -o $@ $^ $(LIBS)

obj/%.o: src/%.c |obj
	@echo CC $<
	@$(CC) $(CFLAGS) $(INCLUDES) -MMD -MF $(@:.o=.d) -c -o $@ $<

obj/graphics.o: src/shaders.inc
-include obj/*.d

src/shaders.inc: src/shader.frag src/shader.vert
	@xxd -i src/shader.frag > src/shaders.inc
	@xxd -i src/shader.vert >> src/shaders.inc

obj:
	@mkdir -p obj

bin:
	@mkdir -p bin
