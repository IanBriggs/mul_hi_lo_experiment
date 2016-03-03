CFLAGS:=-std=c11 -Wall -Werror -Wpedantic -O3 ${CFLAGS} 
export C_INCLUDE_PATH:=include:${C_INCLUDE_PATH}


all: bin/experiment bin/tau_filter.py


bin/experiment: src/main.c include/mul_hi_lo.h
	$(CC) $(CFLAGS) src/main.c -o bin/experiment -lm

bin/toy_32: src/toy.c include/static_assert.h
	$(CC) $(CFLAGS) -DUSE_32_BIT src/toy.c -o bin/toy_32 -lm

bin/toy_64: src/toy.c include/static_assert.h
	$(CC) $(CFLAGS) -DUSE_64_BIT src/toy.c -o bin/toy_64 -lm

bin/tau_filter.py: src/tau_filter.py
	@$(CP) src/tau_filter.py bin/tau_filter.py
	@chmod +X bin/tau_filter.py

.PHONY: clean
clean:
	$(RM) bin/experiment
	$(RM) bin/toy_32
	$(RM) bin/toy_64