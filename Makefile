CC=gcc

CFLAGS=-Wall -Wextra -g3

OFLAGS=-march=native -O2

all: clean cw

cw:
	$(CC) $(CFLAGS) $(OFLAGS) ./src/cachewatch.c -o ./bin/cachewatch

red:
	$(CC) $(CFLAGS) -Ofast ./test/reduc.c -o ./test/reduc

dot:
	$(CC) $(CFLAGS) -Ofast ./test/dotprod.c -o ./test/dotprod

gijk:
	$(CC) $(CFLAGS) -Ofast ./test/dgemm_ijk.c -o ./test/dgemm_ijk

gikj:
	$(CC) $(CFLAGS) -Ofast ./test/dgemm_ikj.c -o ./test/dgemm_ikj

testr: red
	@./bin/cachewatch --start 3
	@taskset -c 3 ./test/reduc 100000000
	@./bin/cachewatch --stop
	@cat cachewatch.out

testd: dot
	@./bin/cachewatch --start 3
	@taskset -c 3 ./test/dotprod 100000000
	@./bin/cachewatch --stop
	@cat cachewatch.out

testgijk: gijk
	@./bin/cachewatch --start 3
	@taskset -c 3 ./test/dgemm_ijk 2000
	@./bin/cachewatch --stop
	@cat cachewatch.out

testgikj: gikj
	@./bin/cachewatch --start 3
	@taskset -c 3 ./test/dgemm_ikj 2000
	@./bin/cachewatch --stop
	@cat cachewatch.out

test: clean cw testr testd testgijk testgikj

clean:
	@rm -Rf ./bin/cachewatch ./cachewatch.out* ./test/reduc ./test/dotprod ./test/dgemm_ijk ./test/dgemm_ikj
