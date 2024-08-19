#include "csapp.h"

void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {   
        Fputs(buf, stdout);
        printf("server recieved %d bytes\n", (int)n);
        Rio_writen(connfd, buf, n);
    }
    
}