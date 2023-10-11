CC = cc
CFLAGS = -fPIC -std=c99 -Wall -Wextra -pedantic

# All SRCDIR subdirectories that contain source files
DIRS = .

SRCDIR = src
OBJDIR = obj
SRCDIRS := $(foreach dir, $(DIRS), $(addprefix $(SRCDIR)/, $(dir)))
OBJDIRS := $(foreach dir, $(DIRS), $(addprefix $(OBJDIR)/, $(dir)))
SRCS := $(foreach dir, $(SRCDIRS), $(wildcard $(dir)/*.c))
OBJS := $(patsubst $(SRCDIR)/%, $(OBJDIR)/%, $(SRCS:.c=.o))
LIBTSP = libtsp.a
LIBSTAPLE = libstaple/libstaple.a

.PHONY: directories all clean debug

all: task1

directories:
	mkdir -p $(SRCDIRS) $(OBJDIRS)

$(LIBTSP): directories $(LIBSTAPLE) $(OBJS)
	@echo 'CREATE $(LIBTSP)' >script.ar
	@echo 'ADDLIB $(LIBSTAPLE)' >>script.ar
	@echo 'ADDMOD $(OBJS)' >>script.ar
	@echo 'SAVE' >>script.ar
	@echo 'END' >>script.ar
	$(AR) -M <script.ar
	@$(RM) script.ar

$(LIBSTAPLE): libstaple
	$(MAKE) -C $^ ABORT=1

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c $(CFLAGS) $^ -o $@

clean:
	$(RM) -- $(LIBTSP) $(OBJS)
	$(MAKE) -C task1 clean

debug: CFLAGS += -g -Og
debug: clean all

task%: $(LIBTSP)
	$(MAKE) -C $@
