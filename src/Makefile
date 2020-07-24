include ../Make.defines

PROGS = main

all: ${PROGS}

main: main.o
	${CC} ${CXXFLAGS} -o $@ $< ${LIBS}

clean:
	rm -f ${CLEANFILES} ${PROGS}
