include makevars.mk

TARGET_DIR ?= build
TARGET = ${TARGET_DIR}/mtrace.so

SRC = mtrace.c
OBJ = ${SRC:.c=.o}

CFLAGS += -fPIC -I ${ERTS_INCLUDE_DIR}
CFLAGS += -Wall -Werror -Wno-parentheses
CFLAGS += -O3 -fno-strict-aliasing

LDFLAGS += -shared

${OBJ}: ${SRC} makevars.mk
	@${CC} ${CFLAGS} -c -o ${OBJ} ${SRC}

${TARGET}: ${OBJ} ${TARGET_DIR} makevars.mk
	@${CC} ${LDFLAGS} -o ${TARGET} ${OBJ} /usr/lib/x86_64-linux-gnu/libunwind.so

${TARGET_DIR}:
	@mkdir -p ${TARGET_DIR}

all: ${TARGET}

clean:
	@rm -fr ${TARGET_DIR} ${OBJ} makevars.mk

.PHONY: all clean
