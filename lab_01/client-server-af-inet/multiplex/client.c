#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/time.h>

#define PORT 1337
#define ITER 10000

int fl = 1;
void sig_handler(int sig_num)
{
    fl = 0;
}

int main(int argc, char **argv)
{
    int sockfd;
    struct sockaddr_in servaddr;
    char client_type = 'p', buf[3];
    ssize_t read_len = 0;

    if (argc != 2)
    {
        printf("usage: %s IP", argv[0]);
        exit(1);
    }
    
    if (signal(SIGINT, sig_handler) == SIG_ERR)
    {
        perror("signal");
        printf("errno: %d\n", errno);
        exit(1);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) == -1)
    {
        perror("inet pton");
        printf("errno: %d\n", errno);
        exit(1);
    }

    for (int i = 0; (i < ITER) && fl; i++)
    {
        if ((i % 5) == 0)
        {
            if (client_type == 'p')
                client_type = 'c';
            else
                client_type = 'p';
        }
        
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1)
        {
            perror("socket");
            printf("errno: %d\n", errno);
            close(sockfd);
            exit(1);
        }
        if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
        {
            perror("connect");
            printf("%d) errno: %d\n", i, errno);
            close(sockfd);
            exit(1);
        }

        if (write(sockfd, &client_type, sizeof(client_type)) == -1)
        {
            perror("write");
            printf("errno: %d\n", errno);
            close(sockfd);
            exit(1);
        }
        
        read_len = read(sockfd, buf, sizeof(buf));
        if (read_len == -1)
        {
            perror("read");
            printf("errno: %d\n", errno);
            close(sockfd);
            exit(1);
        }
        else if (read_len == 0)
        {
            printf("connection terminated\n");
            close(sockfd);\
            exit(1);
        }
        else
        {
            buf[read_len - 1] = '\0';

            printf("Type: %c. Result: %s\n", client_type, buf);
        }
        close(sockfd);
    }
    return 0;
}
