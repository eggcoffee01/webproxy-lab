#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
typedef struct cache_block {
    char url[MAXLINE];
    ssize_t data_length;
    char *response_data;
    struct cache_block *next;
    struct cache_block *prev;
} cache_block;

static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
static const char *http_version = "HTTP/1.0";

int total_cache_size = 0;
cache_block *head_ptr = NULL;
cache_block *tail_ptr = NULL;

void doit(int connfd);
void request(int clientfd, char *method, char *filename, char *host);
ssize_t response(int connfd, int clientfd, char *response_buf);
int parse_uri(char *uri, char *filename, char *host, char *port);
void *thread(void *vargp);
void write_cache(cache_block *block);
cache_block *find_cache(char *cache_check);
void send_cache(cache_block *block, int connfd);

// client -> proxy -> server, server -> proxy -> client
int main(int argc, char **argv) {
    int listenfd, *connfdp;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    // 인자가 하나일때만 실행해(포트번호)
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // 받은 포트번호로 리슨 디스크립터 생성!
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        // 리슨 중인 놈으로부터 연결되면 커넥트 디스크립터 생성!
        clientlen = sizeof(clientaddr);
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);

        Pthread_create(&tid, NULL, thread, connfdp);
    }
}

void *thread(void *vargp) {
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}

// proxy -> server
void doit(int connfd) {
    int clientfd;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char host[MAXLINE], port[MAXLINE];
    char filename[MAXLINE], response_buf[MAX_CACHE_SIZE];
    char cache_check[MAXLINE];
    cache_block *block;
    rio_t rio;
    ssize_t n;

    Rio_readinitb(&rio, connfd); // rio 초기화
    Rio_readlineb(&rio, buf, MAXLINE); // 한 줄 읽기!
    printf("Request headers to proxy:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    parse_uri(uri, filename, host, port);
    strcpy(cache_check, host);
    strcat(cache_check, ":");
    strcat(cache_check, port);
    strcat(cache_check, filename);

    block = find_cache(cache_check);
    if (block) {
        send_cache(block, connfd);
        return;
    }

    clientfd = Open_clientfd(host, port);
    request(clientfd, method, filename, host);
    n = response(connfd, clientfd, response_buf);

    if (n <= MAX_OBJECT_SIZE) {
        cache_block *block = calloc(1, sizeof(cache_block));
        strcpy(block->url, cache_check);
        block->data_length = n;
        block->response_data = malloc(n);
        memcpy(block->response_data, response_buf, n);
        write_cache(block);
    }
    Close(clientfd);
}

int parse_uri(char *uri, char *filename, char *host, char *port) {
    char *ptr;

    if ((ptr = strstr(uri, "://"))) {
        ptr += 3;
        strcpy(host, ptr);
    } else {
        ptr = uri;
        strcpy(host, ptr);
    }

    if ((ptr = strchr(host, '/'))) {
        *ptr = '\0';
        ptr += 1;
        strcpy(filename, "/");
        strcat(filename, ptr);
    } else {
        strcpy(filename, "/");
    }

    /* port 추출 */
    if ((ptr = strchr(host, ':'))) {
        *ptr = '\0';
        ptr += 1;
        strcpy(port, ptr);
    } else {
        strcpy(port, "80");
    }

    return 0;
}

void request(int clientfd, char *method, char *filename, char *host) {
    char buf[MAXLINE];
    printf("Request headers to server:\n");
    printf("%s %s %s\n", method, filename, http_version);

    sprintf(buf, "GET %s %s\r\n", filename, http_version);
    sprintf(buf, "%sHost: %s\r\n", buf, host);
    sprintf(buf, "%s%s", buf, user_agent_hdr);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sProxy-Connection: close\r\n\r\n", buf);

    Rio_writen(clientfd, buf, (size_t)strlen(buf));
}

ssize_t response(int connfd, int clientfd, char *response_buf) {
    rio_t rio;
    ssize_t n;

    Rio_readinitb(&rio, clientfd);
    n = Rio_readnb(&rio, response_buf, MAX_CACHE_SIZE);
    Rio_writen(connfd, response_buf, n);
    return n;
}

void send_cache(cache_block *block, int connfd) {
    Rio_writen(connfd, block->response_data, block->data_length);
}

cache_block *find_cache(char *cache_check) {
    if (!head_ptr) return NULL;

    cache_block *temp = head_ptr;
    while (temp) {
        if (!strcmp(temp->url, cache_check)) {
            return temp;
        }
        temp = temp->next;
    }
    return NULL;
}

void write_cache(cache_block *block) {
    total_cache_size += block->data_length;

    if (total_cache_size > MAX_CACHE_SIZE) {
        total_cache_size -= tail_ptr->data_length;
        tail_ptr = tail_ptr->prev;
        free(tail_ptr->next);
        tail_ptr->next = NULL;
    }

    if (!head_ptr) {
        tail_ptr = block;
    }

    if (head_ptr) {
        head_ptr->prev = block;
        block->next = head_ptr;
    }

    head_ptr = block;
}
