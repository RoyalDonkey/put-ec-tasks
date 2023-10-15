CC = cc
CFLAGS = -fPIC -std=gnu99 -Wall -Wextra -Werror -pedantic -O2

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
TASKS = task1

.PHONY: directories all clean debug profile

all: directories $(TASKS)

directories:
	@mkdir -p $(SRCDIRS) $(OBJDIRS)

$(LIBTSP): $(LIBSTAPLE) $(OBJS)
	@echo 'CREATE $(LIBTSP)' >script.ar
	@echo 'ADDLIB $(LIBSTAPLE)' >>script.ar
	@echo 'ADDMOD $(OBJS)' >>script.ar
	@echo 'SAVE' >>script.ar
	@echo 'END' >>script.ar
	$(AR) -M <script.ar
	@$(RM) script.ar
	@for t in $(TASKS); do $(MAKE) -C "$$t" clean; done

$(LIBSTAPLE):
	@$(MAKE) -C libstaple ABORT=1

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c $(CFLAGS) $^ -o $@

clean:
	$(RM) -- $(LIBTSP) $(OBJS)
	@for t in $(TASKS); do $(MAKE) -C "$$t" clean; done

debug: CFLAGS += -g -Og
debug: clean all

profile: CFLAGS += -pg
profile: LDFLAGS += -pg
profile: clean all

task%: $(LIBTSP)
	@$(MAKE) -C $@
