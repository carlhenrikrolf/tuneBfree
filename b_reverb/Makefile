tests: reverb.h reverb.cpp
	clang++ \
	-g \
	-Wall \
	-fsanitize=address,undefined \
	-o tests \
	-DTESTS \
	-I../libs/doctest/doctest \
	reverb.cpp \

	./tests

clean:
	rm -f tests
