#define _POSIX_C_SOURCE 200809L
#include <sys/socket.h>
#include <poll.h>
#include <stdarg.h>
#include <string.h>
#define ioctl mock_ioctl
#include "../b004link.c"
#undef ioctl
static int readiness_error;
int mock_ioctl(int fd, unsigned long request, ...)
{
    struct pollfd p;
    if (readiness_error) { errno = EIO; return -1; }
    p.fd = fd;
    p.events = request == LINKREADABLE ? POLLIN : POLLOUT;
    p.revents = 0;
    return poll(&p, 1, 0) > 0;
}
int main(int argc, char **argv)
{
    int fd[2], n;
    char buffer[8];
    struct timespec start, end;
    double elapsed;
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd)) return 2;
    clock_gettime(CLOCK_MONOTONIC, &start);
    if (!strcmp(argv[1], "short")) write(fd[1], "abc", 3);
    if (!strcmp(argv[1], "error")) readiness_error = 1;
    if (!strcmp(argv[1], "write")) {
        n = WriteLink(fd[0], "abcdefgh", 8, 1);
        if (n != 8 || read(fd[1], buffer, 8) != 8 || memcmp(buffer, "abcdefgh", 8)) return 2;
    } else n = ReadLink(fd[0], buffer, sizeof(buffer), 1);
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (double)(end.tv_sec-start.tv_sec) + (double)(end.tv_nsec-start.tv_nsec)/1e9;
    printf("%d %.6f\n", n, elapsed);
    close(fd[0]); close(fd[1]);
    return 0;
}
