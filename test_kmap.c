#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFSIZE 16*1024

int test_mmap(const char* device)
{
    int i = 0, fd = 0, len = 0; 

    char *mptr = NULL;
    char buffer [BUFSIZE];
    const char *str = "Hello world";

    /* device as indicated by user */
    fd = open(device, O_RDWR | O_SYNC);
    if (fd == -1) {
        fprintf( stderr, "Open error on \"%s\"\n", device);
        return fd;
    } else fprintf(stdout, "DEVICE     = '%s'\n", device);


    /**
     * Requesting mmaping  at offset 0 on device memory, last argument.
     * This is used by driver to map device memory. First argument is the
     * virtual memory location in user address space where application
     * wants to setup the mmaping. Normally, it is set to 0  to allow
     * kernel to pick the best location in user address space. Please
     * review man pages: mmap(2)
     */

    mptr = mmap(0, BUFSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mptr == MAP_FAILED) {
        fprintf(stderr, "mmap() failed\n");
        return -1;
    }

    /**
     * Now mmap memory region can be access as user memory. 
     * No syscall overhead
     */

    /* read from mmap region */
    printf("DEBUG PTR  = %p\nDEBUG BUF  = %d\n", mptr, (int)BUFSIZE);
    memset(buffer, 0, BUFSIZE);
    memcpy(buffer, mptr, BUFSIZE - 1);   /* Reading from kernel */
    printf("READ MMAP  = '%s'\n", buffer);

    /* write to mmap region */
    memcpy(mptr, str, strlen(str) + 1); 
    memset(buffer, 0, BUFSIZE);
    memcpy(buffer, mptr, BUFSIZE - 1);
    printf("WRITE MMAP = '%s'\n", buffer);

    munmap(mptr, BUFSIZE);
    close(fd);
}

//-----------------------------------------------------------------------------
void usage(const char* exe) 
{
    printf("Usage: %s <mapping device>\n", exe); 
    printf("mapping devices: 1) /dev/kmap\n"); 
    printf("                 2) /dev/vmap\n"); 

    exit(1); 
}

int main (int argc, const char * argv [])
{
    if (argc < 2 || argc > 3) 
        usage(argv[0]);

    return(test_mmap(argv[1]));
}
