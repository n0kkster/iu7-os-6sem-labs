#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define LOG_FNAME "../mp.txt"

#define THREAD_DELAY 100000

#define PERMS (S_IRWXU | S_IRWXG | S_IRWXO)
#define NP 3
#define NC 3
#define SE 0
#define SF 1
#define SB 2

struct sembuf start_produce[2] = {
    { SE, -1, 0 },
    { SB, -1, 0 }
};
struct sembuf stop_produce[2] = {
    { SB, 1, 0 },
    { SF, 1, 0 }
};
struct sembuf start_consume[2] = {
    { SF, -1, 0 },
    { SB, -1, 0 }
};
struct sembuf stop_consume[2] = {
    { SB, 1, 0 },
    { SE, 1, 0 }
};

#define SERVER_PORT 1338
#define RING_BUF_SIZE 26
#define BUF_SIZE 2

char ring_buffer[RING_BUF_SIZE];
int producer_index = 0;
int consumer_index = 0;
int semfd;

FILE *f;

int flag = 1;

void *produce(void *args)
{
    struct timespec time;
    unsigned long long start, end;
    char buffer[BUF_SIZE];
    int connfd = *((int *)args);

    clock_gettime(CLOCK_REALTIME, &time);
    start = time.tv_nsec;

    int status = 0;
    if (semop(semfd, start_produce, 2) == -1)
    {
        perror("semop");
        printf("errno: %d\n", errno);
        exit(1);
    }
    ring_buffer[producer_index] = 'a' + producer_index;
    ++producer_index;
    if (producer_index >= RING_BUF_SIZE)
        producer_index = 0;
    status = 0;
    if (send(connfd, &status, sizeof(status), 0) == -1)
    {
        perror("send");
        printf("errno: %d\n", errno);
        exit(1);
    }
    if (semop(semfd, stop_produce, 2) == -1)
    {
        perror("semop");
        printf("errno: %d\n", errno);
        exit(1);
    }

    clock_gettime(CLOCK_REALTIME, &time);
    end = time.tv_nsec;
    fprintf(f, "%lf\n", (end - start) / 1000.0);

    usleep(THREAD_DELAY);
    return NULL;
}

void *consume(void *args)
{
    struct timespec time;
    unsigned long long start, end;
    char buffer[BUF_SIZE];
    int connfd = *((int *)args);
    clock_gettime(CLOCK_REALTIME, &time);
    start = time.tv_nsec;

    if (semop(semfd, start_consume, 2) == -1)
    {
        perror("semop");
        printf("errno: %d\n", errno);
        exit(1);
    }
    buffer[1] = ring_buffer[consumer_index];
    ++consumer_index;
    if (consumer_index >= RING_BUF_SIZE)
        consumer_index = 0;

    if (send(connfd, buffer, BUF_SIZE, 0) == -1)
    {
        perror("send");
        printf("errno: %d\n", errno);
        exit(1);
    }
    if (semop(semfd, stop_consume, 2) == -1)
    {
        perror("semop");
        printf("errno: %d\n", errno);
        exit(1);
    }

    clock_gettime(CLOCK_REALTIME, &time);
    end = time.tv_nsec;
    fprintf(f, "%lf\n", (end - start) / 1000.0);

    usleep(THREAD_DELAY);
    return NULL;
}

// void *process_client(void *args)
// {
//     unsigned index;
//     int connfd = *((int *)args);
//     char buffer[BUF_SIZE];
//     int status;
//     int full;
//
//     while (flag)
//     {
//         if (recv(connfd, buffer, BUF_SIZE, 0) == -1)
//         {
//             perror("recv");
//             printf("errno: %d\n", errno);
//             break;
//         }
//         switch (buffer[0])
//         {
//             case 'c':
//                 consume(connfd, buffer);
//                 break;
//
//             case 'p':
//                 produce(connfd, buffer);
//                 break;
//         }
//     }
//     // usleep(25000);
//     return NULL;
// }

void sig_handler(int sig_num)
{
    printf("catch sig %d\n", sig_num);
    flag = 0;
}

int main()
{
    printf("%d\n", getpid());
    key_t semkey;
    int connfd;
    int server_sock;
    socklen_t client_len;
    struct sockaddr_in client_addr, server_addr;
    pthread_t th;
    struct sigaction sa;
    char buffer[BUF_SIZE];

    if ((f = fopen(LOG_FNAME, "w")) == NULL)
    {
        perror("signal");
        printf("errno: %d\n", errno);
        fclose(f);
        exit(1);
    }
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction");
        printf("errno: %d\n", errno);
        fclose(f);
        exit(1);
    }
    if ((semkey = ftok("./keyfile", 1)) == -1)
    {
        perror("ftok\n");
        printf("errno: %d\n", errno);
        // fclose(f);
        exit(1);
    }
    if ((semfd = semget(semkey, 3, IPC_CREAT | PERMS)) == -1)
    {
        perror("semget\n");
        printf("errno: %d\n", errno);
        fclose(f);
        exit(1);
    }
    if (semctl(semfd, SE, SETVAL, 4096) == -1)
    {
        perror("semctl E\n");
        printf("errno: %d\n", errno);
        fclose(f);
        exit(1);
    }
    if (semctl(semfd, SF, SETVAL, 0) == -1)
    {
        perror("semctl F\n");
        printf("errno: %d\n", errno);
        fclose(f);
        exit(1);
    }
    if (semctl(semfd, SB, SETVAL, 1) == -1)
    {
        perror("semctl B\n");
        printf("errno: %d\n", errno);
        fclose(f);
        exit(1);
    }

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1)
    {
        perror("socket");
        printf("errno: %d\n", errno);
        fclose(f);
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        perror("setsockopt");

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind");
        printf("errno: %d\n", errno);
        fclose(f);
        exit(1);
    }

    if (listen(server_sock, 1024) == -1)
    {
        perror("listen");
        printf("errno: %d\n", errno);
        fclose(f);
        exit(1);
    }

    printf("listening on port %d\n", SERVER_PORT);

    while (flag)
    {
        client_len = sizeof(client_addr);
        connfd = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (connfd == -1)
        {
            perror("accept");
            printf("errno: %d\n", errno);

            if (errno != EINTR)
            {
                fclose(f);
                exit(1);
            }
        }
        else
        {
            if (recv(connfd, buffer, BUF_SIZE, 0) == -1)
            {
                perror("recv");
                printf("errno: %d\n", errno);
                break;
            }
            switch (buffer[0])
            {
                case 'c':
                    if (pthread_create(&th, NULL, consume, &connfd) != 0)
                    {
                        perror("pthread_create");
                        printf("errno: %d\n", errno);
                        fclose(f);
                        exit(1);
                    }
                    break;

                case 'p':
                    if (pthread_create(&th, NULL, produce, &connfd) != 0)
                    {
                        perror("pthread_create");
                        printf("errno: %d\n", errno);
                        fclose(f);
                        exit(1);
                    }
                    break;
            }

            // if (pthread_create(&th, NULL, process_client, &connfd) != 0)
            // {
            //     perror("pthread_create");
            //     printf("errno: %d\n", errno);
            //     // fclose(f);
            //     exit(1);
            // }

            if (pthread_detach(th))
            {
                perror("pthread_detach");
                printf("errno: %d\n", errno);
                // fclose(f);
                exit(1);
            }
        }
    }

    if (semctl(semfd, 0, IPC_RMID) == -1)
    {
        perror("semctl\n");
        printf("errno: %d\n", errno);
        fclose(f);
        exit(1);
    }

    fclose(f);
}
