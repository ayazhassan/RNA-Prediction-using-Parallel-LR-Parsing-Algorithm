all: stack.o mpi_parser.o mpi_parser

stack.o: stack.cc stack.h
	mpiicc -no-vec -w0 -c stack.cc -o stack.o
	
mpi_parser.o: mpi_parser.cc stack.h
	mpiicc -no-vec -w0 -c mpi_parser.cc -o mpi_parser.o
	
mpi_parser: mpi_parser.o stack.o
	mpiicc -no-vec -w0 -o mpi_parser mpi_parser.o stack.o
	
clean:
	rm mpi_parser
	rm *.o
	