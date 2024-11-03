#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

int open_tcp(int type, char* ip, int port);
int open_tcp_serv(int type, int port);
int close_tcp(int fd);