# Target library
lib := libfs.a

all: $(lib)

objs	:= fs.o disk.o
CC	:= gcc
CFLAGS	:= -Wall
CFLAGS 	+= -g

ifneq ($(V),1)
Q = @
endif

deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)

libfs.a: $(objs)
	ar rcs libfs.a $(objs)

%.o: %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo "clean"
	$(Q)rm -f $(lib) $(objs) $(deps)
