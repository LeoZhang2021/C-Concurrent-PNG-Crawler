# Compiler
CC := gcc
LD := gcc

# Flags
CFLAGS_XML2 = $(shell xml2-config --cflags)
CFLAGS_CURL = $(shell curl-config --cflags)
CFLAGS = -Wall $(CFLAGS_XML2) $(CFLAGS_CURL) -std=gnu99 -g
LDFLAGS := -g -std=gnu99
LDLIBS_XML2 = $(shell xml2-config --libs)
LDLIBS_CURL = $(shell curl-config --libs)
LDLIBS := $(LDLIBS_XML2) $(LDLIBS_CURL)

# Directories & Extensions
SRCDIR := src
INCDIR := src/include
OBJDIR := objects
DEPDIR := dependency
SRCEXT := c

# Sources & Objects
SRCS := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
LIBS := $(shell find $(INCDIR) -type f -name *.h)
OBJS := $(patsubst $(SRCDIR)/%, $(OBJDIR)/%.o, $(basename $(SRCS)))
LIB_UTILS := $(patsubst $(INCDIR)/%, $(OBJDIR)/%.o, $(basename $(LIBS)))

# Target File Name
FINDPNG = $(LIB_UTILS) $(OBJDIR)/findpng3.o
TARGETS = findpng3

# Make
all: ${TARGETS}

findpng3: $(FINDPNG)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.$(SRCEXT) | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $^

$(DEPDIR)/%.d: $(SRCDIR)/%.$(SRCEXT) | $(DEPDIR)
	gcc -MM -MF $@ $^

-include $(SRCS:$(SRCDIR)/%.$(SRCEXT)=$(DEPDIR)/%.d)

$(OBJDIR):
	mkdir -p $@

$(DEPDIR):
	mkdir -p $@

# Remake
#remake: clean all

# Remove
.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(DEPDIR) $(TARGETS)
