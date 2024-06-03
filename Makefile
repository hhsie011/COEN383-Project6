all:
	gcc -o main src/main.c
	./main

clean:
	rm main
