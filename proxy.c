#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */

static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";
static const char *http_version = "HTTP/1.0";

void doit(int connfd);
void request(int clientfd, char *method, char *filename, char *host);
void response(int connfd, int clientfd);
int parse_uri(char *uri, char *filename, char *host, char *port);
void *thread(void *vargp);


//client -> proxy -> server, server -> proxy, -> client
int main(int argc, char **argv) {
  int listenfd, *connfdp; //리슨, 연결 디스크립터
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen; //clientaddr size
  struct sockaddr_storage clientaddr; //clientaddr 
  pthread_t tid;
  // 인자가 하나일때만 실행해(포트번호)
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  
  //받은 포트번호로 리슨디스크립터 생성!
  listenfd = Open_listenfd(argv[1]);
  while (1) {
    // 리슨중인놈으로 부터 연결되면 커넥트디스크립터 생성!
    clientlen = sizeof(clientaddr);
    connfdp = Malloc(sizeof(int));
    *connfdp = Accept(listenfd, (SA *) &clientaddr, &clientlen);
    
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    
    Pthread_create(&tid, NULL, thread, connfdp);
  }
}  

void *thread(void *vargp){
  int connfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  doit(connfd);
  Close(connfd);
  return NULL;
}

//proxy -> server
void doit(int connfd){
  int clientfd;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char host[MAXLINE], port[MAXLINE];
  char filename[MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, connfd); //rio 초기화
  Rio_readlineb(&rio, buf, MAXLINE); //한 줄 읽기!
  printf("Request headers to proxy:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  parse_uri(uri, filename, host, port);

  clientfd = Open_clientfd(host, port);

  printf("%s %s\n", host, port);  
  request(clientfd, method, filename, host);
  response(connfd, clientfd);
  Close(clientfd);
}

int parse_uri(char *uri, char *filename, char *host, char *port){
  char *ptr;

  
  if (!(ptr = strstr(uri, "://"))) 
    return -1;                        
  ptr += 3;                       
  strcpy(host, ptr);                  

  
  if((ptr = strchr(host, '/'))){  
    *ptr = '\0';                     
    ptr += 1;
    strcpy(filename, "/");           
    strcat(filename, ptr);           
  }
  else strcpy(filename, "/");

  /* port 추출 */
  if ((ptr = strchr(host, ':'))){    
    *ptr = '\0';                      
    ptr += 1;     
    strcpy(port, ptr);                
  }  
  else strcpy(port, "80");            

  
  return 0; 
}

void request(int clientfd, char *method, char *filename, char *host){
  char buf[MAXLINE];
  printf("Request headers to server: \n");     
  printf("%s %s %s\n", method, filename, http_version);


  sprintf(buf, "GET %s %s\r\n", filename, http_version);
  sprintf(buf, "%sHost: %s\r\n", buf, host);
  sprintf(buf, "%s%s", buf, user_agent_hdr);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sProxy-Connection: close\r\n\r\n", buf);

  Rio_writen(clientfd, buf, (size_t)strlen(buf));
}

void response(int connfd, int clientfd){
  char buf[MAX_CACHE_SIZE];
  rio_t rio;
  ssize_t n;

  Rio_readinitb(&rio, clientfd);
  n = Rio_readnb(&rio, buf, MAX_CACHE_SIZE);
  Rio_writen(connfd, buf, n);
}