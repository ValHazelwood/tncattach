#include "TCP.h"
#include <arpa/inet.h>

int open_tcp(int type, char *ip, int port)
{
    int sockfd = type == 1 ? socket(AF_INET, SOCK_DGRAM, 0) : socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
    {
        perror("Could not open AF_INET socket");
        exit(1);
    }

    struct sockaddr_in serv_addr;

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr.s_addr) == 0)
    {
        perror("Invalid IP address");
        exit(1);
    }

    serv_addr.sin_port = htons(port);

    int errno = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    if (errno < 0)
    {
        perror("Could not connect TCP socket");
        exit(1);
    }

    return sockfd;
}

int open_tcp_serv(int type, int port)
{
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    int n;

    server_sock = type == 1 ? socket(AF_INET, SOCK_DGRAM, 0) : socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0)
    {
        perror("[-]Socket error");
        exit(1);
    }

    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    n = bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (n < 0)
    {
        perror("[-]Bind error");
        exit(1);
    }
    printf("[+]Bind to the port number: %d\n", port);

    if (type != 1)
    {
        listen(server_sock, 5);

        printf("Listening...\n");

        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);

        return client_sock;
    }

    return server_sock;
}

int close_tcp(int fd)
{
    return close(fd);
}
