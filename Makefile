SOURCES = $(wildcard src/*.c)
CFLAGS = -Wall
INCLUDES = -I/usr/include/freetype2
LIBS = -lm -lpthread -lfreetype -lGLESv2 -lEGL

ifndef CC
	CC = gcc
endif

ifdef DEBUG
	CFLAGS += -O1 -g -Werror
else
	CFLAGS += -O2
endif

ifdef X11BUILD
	CFLAGS += -DX11BUILD
	LIBS += -lxcb
endif

ifdef TRACE_LEVEL
	CFLAGS += -DTRACE_LEVEL=$(TRACE_LEVEL)
else
	CFLAGS += -DTRACE_LEVEL=2
endif

.PHONY: all clean

all: bin/arnav

clean:
	@rm -r -f obj bin

bin/arnav:  obj/main.o bin/arnav.a
	@echo LD $@
	@$(CC) -o $@ $^ $(LIBS)

bin/arnav.a: $(filter-out obj/main.o, $(SOURCES:src/%.c=obj/%.o)) |bin
	@echo AR $@
	@ar rcs $@ $^

-include obj/*.d
obj/%.o: src/%.c |obj
	@echo CC $<
	@$(CC) $(CFLAGS) $(INCLUDES) -MMD -MF $(@:.o=.d) -c -o $@ $<

obj:
	@mkdir -p obj

bin:
	@mkdir -p bin
