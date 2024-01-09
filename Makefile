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
LIBGNUPLOT_I = gnuplot_i/gnuplot_i.o
TASKS = task1 task2 task3 task4 task5 task6 task7 task8 task9 task10

.PHONY: directories all clean debug profile fast

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

$(LIBSTAPLE):
	@$(MAKE) -C libstaple ABORT=1

$(LIBGNUPLOT_I):
	$(CC) -c -std=gnu99 -O2 -o gnuplot_i/gnuplot_i.o gnuplot_i/gnuplot_i.c

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c $(CFLAGS) $^ -o $@

clean:
	$(RM) -- $(LIBTSP) $(OBJS)
	$(RM) -- gnuplot_i/gnuplot_i.o
	@for t in $(TASKS); do $(MAKE) -C "$$t" clean; done

debug: CFLAGS += -g -Og -ftrapv
debug: clean all

profile: CFLAGS += -Wno-error -DNDEBUG -pg
profile: LDFLAGS += -pg
profile: clean all

fast: CFLAGS += -Wno-error -DNDEBUG
fast: clean all

task%: $(LIBTSP) $(LIBGNUPLOT_I)
	@$(MAKE) -C $@
