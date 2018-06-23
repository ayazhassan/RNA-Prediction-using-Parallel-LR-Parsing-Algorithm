all: sequential_parser stack.o sequential_parser.o

stack.o: stack.cc stack.h
	icc -no-vec -c stack.cc -o stack.o

sequential_parser.o: sequential_parser.cc stack.h
	icc -no-vec -c sequential_parser.cc -o sequential_parser.o

sequential_parser: sequential_parser.o stack.o
	icc -no-vec -o sequential_parser sequential_parser.o stack.o

clean:
	rm sequential_parser
	rm *.o

