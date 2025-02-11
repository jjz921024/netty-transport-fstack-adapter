TARGET=libnetty_fstack.so

SRCS=netty_fstack.c

CFLAGS+=-I${JAVA_HOME}/include -I${JAVA_HOME}/include/linux -I${FF_PATH}/lib
CFLAGS+=-I/home/skyjiang/java-projects/netty/transport-native-unix-common/src/main/c
CFLAGS+=-I/home/skyjiang/java-projects/netty-jni-util/src/main/c

LIBS+=-L${FF_PATH}/lib -lfstack

# cc -shared -fPIC ${CFLAGS} -o ${TARGET} ${SRCS} ${LIBS}
all:
	echo "hi.........................."

.PHONY: clean
clean:
	rm -f *.o ${TARGET}