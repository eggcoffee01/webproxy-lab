/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  if((buf = getenv("QUERY_STRING")) != NULL){
    //쿼리중 &를 찾고 그 포인터를 반환함.
    p = strchr(buf, '&');
    //&를 \0으로 바꿔서 문자열을 나눔
    *p = '\0';
    strcpy(arg1, buf);
    strcpy(arg2, p+1);

    p = strchr(arg1, '=');
    strcpy(arg1, p+1);
    
    p = strchr(arg2, '=');
    strcpy(arg2, p+1);
    
    n1 = atoi(arg1);
    n2 = atoi(arg2);
  }

  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sThe Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1+n2);
  sprintf(content, "%sThanks for visiting!\r\n<p>", content);

  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  // 출력 버퍼 모아서 발사!
  fflush(stdout);

  exit(0);
}
/* $end adder */
