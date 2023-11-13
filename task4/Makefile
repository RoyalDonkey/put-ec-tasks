CC = cc
LINKER = cc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -O2
LDFLAGS = -lm -L.. -l:libtsp.a

# All SRCDIR subdirectories that contain source files
DIRS = .

SRCDIR = src
OBJDIR = obj
SRCDIRS := $(foreach dir, $(DIRS), $(addprefix $(SRCDIR)/, $(dir)))
OBJDIRS := $(foreach dir, $(DIRS), $(addprefix $(OBJDIR)/, $(dir)))
SRCS := $(foreach dir, $(SRCDIRS), $(wildcard $(dir)/*.c))
OBJS := $(patsubst $(SRCDIR)/%, $(OBJDIR)/%, $(SRCS:.c=.o))
TARGET = task4

.PHONY: directories all main clean debug profile fast

all: directories $(TARGET)

directories:
	@mkdir -p $(SRCDIRS) $(OBJDIRS)

$(TARGET): $(OBJS)
	$(LINKER) $(OBJS) $(LDFLAGS) -o $(TARGET)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c $(CFLAGS) $^ -o $@

clean:
	$(RM) -- $(TARGET) $(OBJS)

debug: CFLAGS += -g -Og -ftrapv
debug: clean all

fast: CFLAGS += -Wno-error -DNDEBUG
fast: clean all

profile: CFLAGS += -pg
profile: LDFLAGS += -pg
profile: clean all
