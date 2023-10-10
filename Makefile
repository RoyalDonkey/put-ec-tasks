CC=cc
CFLAGS=-fPIC -std=c99 -Wall -Wextra -pedantic

# All SRCDIR subdirectories that contain source files
DIRS=.

SRCDIR=src
OBJDIR=obj
SRCDIRS:=$(foreach dir, $(DIRS), $(addprefix $(SRCDIR)/, $(dir)))
OBJDIRS:=$(foreach dir, $(DIRS), $(addprefix $(OBJDIR)/, $(dir)))
SRCS:=$(foreach dir, $(SRCDIRS), $(wildcard $(dir)/*.c))
OBJS:=$(patsubst $(SRCDIR)/%, $(OBJDIR)/%, $(SRCS:.c=.o))

.PHONY: directories all clean debug

all: task1

directories:
	mkdir -p $(SRCDIRS) $(OBJDIRS)

libtsp.a: directories libstaple.a $(OBJS)
	@echo 'CREATE libtsp.a' >script.ar
	@echo 'ADDLIB libstaple.a' >>script.ar
	@echo 'ADDMOD $(OBJS)' >>script.ar
	@echo 'SAVE' >>script.ar
	@echo 'END' >>script.ar
	$(AR) -M <script.ar
	@$(RM) script.ar

libstaple.a: libstaple
	$(MAKE) -C $^ ABORT=1
	ln -sf libstaple/libstaple.a libstaple.a

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c $(CFLAGS) $^ -o $@

clean:
	$(RM) -- $(OBJS) libstaple.a
	$(MAKE) -C task1 clean

debug: CFLAGS += -g -Og
debug: clean all

task%: libtsp.a libstaple.a
	$(MAKE) -C $@
