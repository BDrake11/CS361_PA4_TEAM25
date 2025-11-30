all: procurement  factory

procurement: procurement.c  wrappers.c  wrappers.h message.c message.h
	gcc -pthread  procurement.c  wrappers.c  message.c  -o procurement

factory: factory.c  wrappers.c  wrappers.h message.c  message.h
	gcc -pthread  factory.c     wrappers.c  message.c  -o factory

clean:
	rm -f *.o  factory procurement *.log
	rm -f /dev/shm/*
