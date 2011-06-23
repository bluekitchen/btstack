#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {

    if (argc < 2) {
         printf("Usage: %s fileToPath\n", argv[0]);
         printf("This tool disables the 'csr -T' configuration from BlueTool.\n");
         return 10;
    }
    printf("Patching %s...\n", argv[1]);

    struct stat sb;
	int fd = open (argv[1], O_RDWR);
	if (fd < 0) return 10;
	if (fstat (fd, &sb) == -1) {
		close(fd);
		return 10;
	}
	uint32_t len = sb.st_size;
	uint8_t * p = mmap (0, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		close(fd);
		return 10;
	}
	
	int count = 0;
	
	// fix Apple's horrible capitalization :)
	// const char *pattern =    "BlueTool";
	// const char *patternNew = "Bluetool";
    
    // comment ("csr -T")
    const char *pattern =    "csr -T";
	const char *patternNew = "######";
	const int patternLen = strlen(pattern);
    int i;
    for (i=0; i<len-patternLen; i++){
        if (strncmp((char*)p+i, pattern, patternLen) == 0){
            // comment
            strncpy((char*)p+i, patternNew, patternLen);        
            count++;
        }
    }
    	
	// done
	munmap(p,len);
	close(fd);
    
    printf("Done, replaced %s %u times\n", pattern, count);
    return 0;
}