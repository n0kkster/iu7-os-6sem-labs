#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define SERVER_PORT 1338
#define BUF_SIZE 2
#define RING_BUF_SIZE 26

int flag = 1;

void sig_handler(int sig_num)
{
    printf("catch sig %d\n", sig_num);
    flag = 0;
}

void request(int sockfd, char mode)
{
    int index;
    char buffer[BUF_SIZE];
    buffer[0] = mode;

    if (send(sockfd, buffer, sizeof(buffer), 0) == -1)
    {
        perror("send");
        exit(1);
    }

    if (recv(sockfd, buffer, sizeof(buffer), 0) == -1)
    {
        perror("recieve");
        exit(1);
    }

    if (mode == 'c')
    {
        printf("[C] %d recv: %c\n", getpid(), buffer[1]);
    }
    else
    {
        printf("[P] %d recv: %s\n", getpid(), buffer[1] == 0 ? "OK" : "ERR");
    }
    usleep(20000 + rand() % 15000);
}

int main(int argc, char **argv)
{
    int sockfd;
    struct sockaddr_in servaddr;

    if (argc != 3)
    {
        printf("usage: %s IP p/c", argv[0]);
        exit(1);
    }

    if (signal(SIGALRM, sig_handler) == SIG_ERR)
    {
        perror("signal");
        printf("errno: %d\n", errno);
        exit(1);
    }

    for (int i = 0; i < 400; i++)
    {
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        {
            perror("socket");
            printf("errno: %d\n", errno);
            exit(1);
        }

        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(SERVER_PORT);
        if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) == -1)
        {
            perror("inet pton");
            printf("errno: %d\n", errno);
            exit(1);
        }

        if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
        {
            perror("connect");
            printf("errno: %d\n", errno);
            exit(1);
        }

        request(sockfd, i % 2 == 0 ? 'p' : 'c');
    }

    close(sockfd);
    exit(0);
}
