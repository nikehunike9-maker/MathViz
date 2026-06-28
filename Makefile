CC      ?= gcc
CFLAGS  ?= -std=c99 -D_GNU_SOURCE -O2 -g -Wall $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
LDFLAGS ?= -lm $(shell pkg-config --libs gtk+-3.0 2>/dev/null)

OBJDIR  = obj
SRCS    = $(filter-out font.c text_renderer.c, $(wildcard *.c))
OBJS    = $(patsubst %.c, $(OBJDIR)/%.o, $(SRCS))
TARGET  = mathviz

.PHONY: all clean run

all: $(OBJDIR) $(TARGET)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(OBJDIR) $(TARGET)

run: $(TARGET)
	./$(TARGET)
