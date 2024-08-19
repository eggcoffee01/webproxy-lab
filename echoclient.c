#include "csapp.h"

int main(int argc, char **argv)
{
    int clientfd;// 클라이언트 파일 디스크립터 선언
    char *host, *port, buf[MAXLINE];//호스트이름, 포트, 버프 선언
    rio_t rio;//입출력 변수

    // 인자 잘못 입력되면 에러출력 후 종료!
    if(argc != 3){
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }
    host = argv[1];
    port = argv[2];

    //소켓 할당
    clientfd = Open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd);

    while (Fgets(buf, MAXLINE, stdin) != NULL)
    {
        Rio_writen(clientfd, buf, strlen(buf));
        Rio_readlineb(&rio, buf, MAXLINE);
        Fputs(buf, stdout);
    }
    Close(clientfd);
    exit(0);
}