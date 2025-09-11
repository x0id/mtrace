all: makevars.mk
	@${MAKE} -f build.mk all

clean:
	@[ ! -e makevars.mk ] || ${MAKE} -f build.mk clean

makevars.mk: makevars.sh
	@./makevars.sh > makevars.mk

.PHONY: all clean
