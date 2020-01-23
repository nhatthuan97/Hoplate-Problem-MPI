
p5: p5.c
	mpicc -Ofast -g p5.c -o p5 -lm

clean:
	rm -f p5
