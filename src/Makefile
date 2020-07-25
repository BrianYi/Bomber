include ../Make.defines

PROGS = bomber

all: ${PROGS}

bomber: bomber.o
	${CC} ${CXXFLAGS} -o $@ $< ${LIBS}

clean:
	rm -f ${CLEANFILES} ${PROGS}
