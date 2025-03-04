CPFLAGS=-I./
LDFLAGS=-L./lib
CFLAGS=-Wall

# Add debug flag for compilation
DEBUG_FLAGS=-DSCHNEIDER_DEBUG

all : bin/gest_ress clients bin/example_schneider libs

bin/gest_ress : src/gest_ress.c
	gcc $(CFLAGS) $(CPFLAGS) $(LDFLAGS) $^ -o $@ -Iinclude

clients: bin/client_train bin/client_train_1 bin/client_train_2 bin/client_train_3 bin/client_train_4

bin/client_train : src/client_train.c
	gcc $(CFLAGS) $(CPFLAGS) $(LDFLAGS) $^ -o $@

bin/client_train_1 : src/client_train_1.c lib/libschneider.so
	gcc $(CFLAGS) $(CPFLAGS) $(LDFLAGS) $^ -o $@ 

bin/client_train_2 : src/client_train_2.c lib/libschneider.so
	gcc $(CFLAGS) $(CPFLAGS) $(LDFLAGS) $^ -o $@ 

bin/client_train_3 : src/client_train_3.c lib/libschneider.so
	gcc $(CFLAGS) $(CPFLAGS) $(LDFLAGS) $^ -o $@ 

bin/client_train_4 : src/client_train_4.c lib/libschneider.so
	gcc $(CFLAGS) $(CPFLAGS) $(LDFLAGS) $^ -o $@ 

libs: lib/libschneider.so

bin/example_schneider: src/example_schneider.c lib/libschneider.so
	gcc $(CFLAGS) $(CPFLAGS) $(LDFLAGS) $^ -o $@

lib/libschneider.so: build/schneider-fpic.o
	gcc -shared $^ -o $@

build/schneider-fpic.o: src/schneider.c
	gcc -c -fPIC $(CFLAGS) $(CPFLAGS) $< -o $@

debug: CFLAGS += $(DEBUG_FLAGS)
debug: clean clean-libs all

clean:
	rm -f bin/* build/*

clean-libs:
	rm -f lib/* build/*
