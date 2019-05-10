all: clean nanobenchFork

nanobenchFork:
	gcc -o nanobenchFork usingNanobench.c nanoBenchCat.c common/nanoBench.c
	gcc -fPIC -shared -o nanobench.so nanoBenchCat.c common/nanoBench.c

clean:
	rm -f nanobenchFork
	rm -f nanobench.so


