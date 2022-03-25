sim: shell.c pipe.c bp.c cache.c
	@gcc -g -O2 $^ -o $@
	cp sim ../

cache_test: cache.c cache.h test_cache.c
	@gcc -g -O2 $^ -o $@

.PHONY: clean
clean:
	rm -rf *.o *~ sim
