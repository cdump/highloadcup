-include Makefile.local

# LFLAGS += -fsanitize=thread
# CFLAGS += -fsanitize=thread
# CC = clang-5.0
# LFLAGS += -fsanitize=memory -fno-omit-frame-pointer
# CFLAGS += -fsanitize=memory -fno-omit-frame-pointer
# CFLAGS += -march=native
# LFLAGS += -march=native
# CFLAGS += -pg -no-pie
# LFLAGS += -pg -no-pie
# CFLAGS += -g
CFLAGS += -DNDEBUG

TARGET = server

BUILDDIR = build
SRCDIR   = src
INCDIR   = include ./src
TPDIR    = third_party
DEPFILE  = $(BUILDDIR)/.depends

CFLAGS += -Wall -std=gnu99 -O3
CFLAGS += $(foreach d, $(shell find $(INCDIR) -type d),-I$d)
CFLAGS += $(foreach d, $(wildcard $(TPDIR)/*),-I$d) -I$(TPDIR)

LFLAGS += -lpthread
# LFLAGS += -lrt

SOURCES := $(shell find src/ -name '*.c')
OBJECTS := $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

TP_SOURCES := $(wildcard $(TPDIR)/*/*.c)
TP_OBJECTS := $(TP_SOURCES:$(TPDIR)/%.c=$(BUILDDIR)/$(TPDIR)/%.o)

ALL_OBJECTS = $(OBJECTS) $(TP_OBJECTS)

$(TARGET): $(ALL_OBJECTS)
	$(CC) -o $@ $(ALL_OBJECTS) $(LFLAGS)

ifneq (clean, $(MAKECMDGOALS))
-include $(DEPFILE)
endif

$(DEPFILE):
	$(CC) -MM $(CFLAGS) $(SOURCES) | sed -e 's,^[^:]\+: \([^/]\+\)/\([^ ]\+\)\.c,$(BUILDDIR)/\2.o: \1/\2.c,' > $(DEPFILE)

$(OBJECTS): $(BUILDDIR)/%.o : $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TP_OBJECTS): $(BUILDDIR)/%.o : %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clangcomp:
	@echo $(CFLAGS) | sed -e 's/ -/\n-/g' | grep -v 'Werror' > .clang_complete

clean:
	rm -f $(ALL_OBJECTS)
	rm -f $(DEPFILE)
	rm -f $(TARGET)

.PHONY: clangcomp clean
