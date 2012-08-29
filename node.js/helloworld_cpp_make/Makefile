all: hello.node helloworld.node

CFLAGS = -g  \
    -fPIC  \
    -DPIC  \
    -D_LARGEFILE_SOURCE  \
    -D_FILE_OFFSET_BITS=64  \
    -D_GNU_SOURCE  \
    -I/usr/include/nodejs \
    -DEV_MULTIPLICITY=0 

LDFLAGS = -lv8 -luv -shared

hello.node: hello.o
	g++  ${LDFLAGS} hello.o -o hello.node

hello.o: hello.cpp
	g++  ${CFLAGS} -c -o hello.o hello.cpp

helloworld.node: helloworld.o
	g++  ${LDFLAGS} helloworld.o -o helloworld.node

helloworld.o: helloworld.cpp
	g++  ${CFLAGS} -c -o helloworld.o helloworld.cpp

runhello: hello.node
	node ./test.js

runhelloworld: helloworld.node
	node ./test-helloworld.js

clean:
	rm -f *.o *.node

