all : bin/gest_ress bin/client_train

bin/gest_ress : source/gest_ress.c
	gcc $^ -o $@ -Iinclude

bin/client_train : source/client_train.c
	gcc $^ -o $@

clean:
	rm -f bin/*