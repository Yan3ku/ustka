CPPFLAGS = -D_DEFAULT_SOURCE
CFLAGS   = -ggdb -std=c11 -pedantic -Wextra -Wall ${CPPFLAGS} ${DEBUG}
LDFLAGS  = ${DEBUG}

BIN = prog
SRC = read.c prog.c
OBJ = ${SRC:.c=.o}

all: options ${BIN}

options:
	@echo ${BIN} build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

${BIN}: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f ${BIN} ${OBJ}

.PHONY: all options clean
