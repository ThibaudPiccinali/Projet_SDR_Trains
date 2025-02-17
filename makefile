CPFLAGS=-I./
LDFLAGS=-L./lib
CFLAGS=-Wall

all : bin/gest_ress bin/client_train bin/example_schneider libs

bin/gest_ress : src/gest_ress.c
	gcc $^ -o $@ -Iinclude

bin/client_train : src/client_train.c
	gcc $^ -o $@

libs: lib/libschneider.so

bin/example_schneider: src/example_schneider.c lib/libschneider.so
	gcc $(CFLAGS) $(CPFLAGS) $(LDFLAGS) $^ -o $@

lib/libschneider.so: build/schneider-fpic.o
	gcc -shared $^ -o $@

build/schneider-fpic.o: src/schneider.c
	gcc -c -fPIC $(CFLAGS) $(CPFLAGS) $< -o $@

clean:
	rm -f bin/* build/*

clean-libs:
	rm -f lib/* build/*
