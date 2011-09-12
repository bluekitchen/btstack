CC=g++

CPPFLAGS= -I../include -I../src -I$(CPPUTEST_HOME)/include -g
LD_LIBRARIES = -L$(CPPUTEST_HOME)/lib -lCppUTest -lCppUTestExt

VPATH=../src

DEPS = remote_device_db_memory.h
OBJ = remote_device_db_memory_test.o remote_device_db_memory.o btstack_memory.o linked_list.o

remote-memory: $(OBJ)
	$(CC) $(CPPFLAGS) -o $@ $^ $(LD_LIBRARIES)

clean:
	rm -f remote-memory *.o ../src/*.o
	
