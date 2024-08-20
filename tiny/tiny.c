/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd; //리슨, 연결 디스크립터
  char hostname[MAXLINE], port[MAXLINE]; //클라이언트 호스트네임, 포트
  socklen_t clientlen; //clientaddr size
  struct sockaddr_storage clientaddr; //clientaddr 

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
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept
    // 연결된놈 정보 가져와서 출력해
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    //doit함수 실행해!
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }
}

void doit(int fd){
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);
  // buf to 3 args
  sscanf(buf, "%s %s %s", method, uri, version);
  //GET 메소드가 아니라면 반환
  if(strcasecmp(method, "GET") && strcasecmp(method, "HEAD")){
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  //프로토콜 요청(예:GET) 다음에 오는 헤더 무시해주기(한줄 버퍼에서 빼주기!)
  read_requesthdrs(&rio);
  
  // uri가지고 정적인지, 동적인지 판단 밑 filename, cgiargs 값 채우기
  is_static = parse_uri(uri, filename, cgiargs);
  // 파일경로가 존재하지 않다면 에러발생
  if(stat(filename, &sbuf) < 0){
    clienterror(fd, filename, "404", "Not Found", "Tiny couldn't find this file");
    return;
  }
  // 파일경로가 존재하고 정적 요청을 했다면
  if(!strcasecmp(method, "HEAD")){
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    //정적 컨텐츠 제공
    serve_header(fd, filename, sbuf.st_size);
    return;
  }


  if(is_static){
    // 파일이 일반 파일이 아니거나(or) 파일이 읽기 권한이 없으면 에러발생
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    //정적 컨텐츠 제공
    serve_static(fd, filename, sbuf.st_size);
  }
  // 파일경로가 존재하고 동적 요청을 했다면
  else{
    // 파일이 일반 파일이 아니거나(or) 파일이 실행 권한이 없으면 에러발생
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    //동적 컨텐츠 제공
    serve_dynamic(fd, filename, cgiargs);
  }
}

//에러 출력 해주는 함수
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,char *longmsg){
  char buf[MAXLINE], body[MAXBUF];

  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

//HTTP 요청 형식에 맞게 헤더부분을 다 읽고 무시해준다.
void read_requesthdrs(rio_t *rp){
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")){
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs){
  char *ptr;

  // uri스트링 안에 cgi-bin이라는 서브 스트링이 있다면 그 포인터를 반환함.
  if(!strstr(uri, "cgi-bin")){
    // cgi-bin이라는 서브 스트링이 없다면, 정적 콘텐츠를 요청한 것이므로
    strcpy(cgiargs, "");// cgiargs는 비워줌
    strcpy(filename, ".");
    strcat(filename, uri);//filename은 .[uri] 형식이됨
    // 추가적으로 uri의 마지막이/라면 디렉토리에 있는 home.html파일을 호출하도록 추가해줌
    if(uri[strlen(uri)-1] == '/')
      strcat(filename, "home.html");
    return 1;
  }
  else{
    // cgi-bin이라는 서브스트링이 있다면
    ptr = index(uri, '?'); // ?를 찾아서 포인터 반환
    if(ptr){
      // ?포인터가 있다면
      strcpy(cgiargs, ptr+1); // cgiargs에다가 포인터+1부터 널 포인터 만날때까지 복사
      *ptr = '\0'; //그다음 ?위치를 널포인터로 만들어 문자열을 끊어줌
    }
    else
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_header(int fd, char *filename, int filesize){
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  //filename에 서브 스트링 확장자 보고 파일타입 반환
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);
}

void serve_static(int fd, char *filename, int filesize){
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  //filename에 서브 스트링 확장자 보고 파일타입 반환
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  //읽기 파일로 파일 열기
  srcfd = Open(filename, O_RDONLY, 0);
  // srcp로 파일을 매핑시작.(읽기 가능, 쓰기 불가능)
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  srcp = Malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);
  Close(srcfd);
  //정적 콘텐츠 전송
  Rio_writen(fd, srcp, filesize);
  // 매핑된 영역 해제
  // Munmap(srcp, filesize);
  free(srcp);
}

void get_filetype(char *filename, char *filetype){
  if(strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if(strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if(strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if(strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if(strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs){
  char buf[MAXLINE], *emptylist[] = { NULL };

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  // 자식 프로세스 생성
  if(Fork() == 0){
    //자식 프로세스에서만 실행됨
    setenv("QUERY_STRING", cgiargs, 1); // 환경 변수 설정함.
    Dup2(fd, STDOUT_FILENO); // fd를 표준 출력으로 리디렉션함. printf하는거 다 fd로 전송됨?
    Execve(filename, emptylist, environ); // filename으로 지정된 cgi 프로그램 실행, 자식 프로세스 cgi 프로그램으로 교체됨
  }
  Wait(NULL);
}