#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void *thread_func(void *arg)
{
    int fd = open("alphabet.txt", O_RDONLY);

    char c;
    int fl = 1;

    while (read(fd, &c, 1))
        write(1, &c, 1);

    close(fd);
    pthread_exit(NULL);
}

int main()
{
    pthread_t t1, t2;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&t1, &attr, thread_func, NULL) != 0)
    {
        perror("pthread_create");
        exit(1);
    }

    if (pthread_create(&t2, &attr, thread_func, NULL) != 0)
    {
        perror("pthread_create");
        exit(1);
    }

    pthread_detach(t1);
    pthread_detach(t2);

    pthread_attr_destroy(&attr);
    pthread_exit(NULL);
}
