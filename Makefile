CC=cc
LINKER=cc
CFLAGS=-std=c99 -Wall -Wextra -pedantic
LDFLAGS=-L. -lstaple -shared

# All SRCDIR subdirectories that contain source files
DIRS=.

SRCDIR=src
OBJDIR=obj
SRCDIRS:=$(foreach dir, $(DIRS), $(addprefix $(SRCDIR)/, $(dir)))
OBJDIRS:=$(foreach dir, $(DIRS), $(addprefix $(OBJDIR)/, $(dir)))
SRCS:=$(foreach dir, $(SRCDIRS), $(wildcard $(dir)/*.c))
OBJS:=$(patsubst $(SRCDIR)/%, $(OBJDIR)/%, $(SRCS:.c=.o))
TARGET=libtsp
DESTDIR=
PREFIX=/usr/local
MANPREFIX=$(PREFIX)/share/man

.PHONY: directories all libtsp clean debug

all: directories libtsp

directories:
	mkdir -p $(SRCDIRS) $(OBJDIRS)

libtsp: libstaple.so $(OBJS)
	$(LINKER) $(OBJS) $(LDFLAGS) -o $(TARGET).so

libstaple.so: libstaple
	$(MAKE) -C $^
	ln -s libstaple/libstaple.so libstaple.so

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c $(CFLAGS) $^ -o $@

clean:
	$(RM) -- $(OBJS) libstaple.so

debug: CFLAGS += -g -Og
debug: clean all
