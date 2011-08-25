CC=gcc
CPPFLAGS=-I../include -I../src -g
VPATH=../src
DEPS = remote_device_db_memory.h
OBJ = remote_device_db_memory_test.o remote_device_db_memory.o btstack_memory.o linked_list.o

remote_device_db_memory_test: $(OBJ)
	$(CC) $(CPPFLAGS) -o $@ $^ 

clean:
	rm -f remote_device_db_memory_test *.o
	
