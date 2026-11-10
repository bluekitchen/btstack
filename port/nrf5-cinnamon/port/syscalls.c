/* Minimal newlib system calls for the bare-metal nRF5 Cinnamon port.
 * Output is supplied separately by SEGGER RTT; there is no filesystem,
 * process management, or standard input on this port.
 */
#include <errno.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>

int _close(int file){
    (void) file;
    errno = EBADF;
    return -1;
}

int _fstat(int file, struct stat *st){
    if (file >= 0 && file <= 2){
        st->st_mode = S_IFCHR;
        return 0;
    }
    errno = EBADF;
    return -1;
}

int _isatty(int file){
    if (file >= 0 && file <= 2) return 1;
    errno = EBADF;
    return 0;
}

int _lseek(int file, int offset, int whence){
    (void) file;
    (void) offset;
    (void) whence;
    errno = ESPIPE;
    return -1;
}

int _read(int file, void *buffer, size_t length){
    (void) file;
    (void) buffer;
    (void) length;
    errno = EBADF;
    return -1;
}

int _getpid(void){
    return 1;
}

int _kill(int pid, int signal){
    (void) pid;
    (void) signal;
    errno = EINVAL;
    return -1;
}
