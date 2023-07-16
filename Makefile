CC = gcc

CFLAGS = -Wall -Werror -pedantic -c -g -I ~pn-cs357/Given/Mush/include

LFLAGS = -g -Wall -Werror -pedantic -L ~pn-cs357/Given/Mush/lib64
IN = test



mush2: mush2.c
	$(CC) $(CFLAGS) -o mush2.o mush2.c
	$(CC) $(LFLAGS) -o mush2 mush2.o -lmush
pipes: pipes.c
	gcc -Wall -Werror -pedantic -g -o pipes pipes.c

all: mush2
	
clean:
	rm mush2.o	
