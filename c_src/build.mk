include makevars.mk

TARGET_DIR ?= build
TARGET = ${TARGET_DIR}/mtrace.so

SRC = mtrace.c
OBJ = ${SRC:.c=.o}

CFLAGS += -fPIC -I ${ERTS_INCLUDE_DIR} -I ${ERL_INTERFACE_INCLUDE_DIR}
CFLAGS += -Wall -Werror -Wno-parentheses
CFLAGS += -O3 -fno-strict-aliasing

LDFLAGS += -shared
LDFLAGS += -L ${ERL_INTERFACE_LIB_DIR} -lei -lunwind

${OBJ}: ${SRC} makevars.mk
	@${CC} ${CFLAGS} -c -o ${OBJ} ${SRC}

${TARGET}: ${OBJ} ${TARGET_DIR} makevars.mk
	@${CC} ${LDFLAGS} -o ${TARGET} ${OBJ}

${TARGET_DIR}:
	@mkdir -p ${TARGET_DIR}

all: ${TARGET}

clean:
	@rm -fr ${TARGET_DIR} ${OBJ} makevars.mk

.PHONY: all clean
