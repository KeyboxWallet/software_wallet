#include "rand.h"
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>

uint32_t random32(){
   uint32_t random;
#if defined(OS_LINUX)
    while (
     syscall( SYS_getrandom, &random, 4, 1) < 4 );
#elif defined(OS_APPLE)
    int fd = open("/dev/urandom", O_RDONLY);
    while (
        read (fd, &random, 4) < 4
    );
#elif defined(OS_WIN)


#else
    #error ("Not support OS!");
#endif
    return random;
}

