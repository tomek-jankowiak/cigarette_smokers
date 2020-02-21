all: main.c
	gcc main.c -o run -Wall

clean:
	rm -f *.o run
