//
// Created by suxin on 2022/6/13.
//

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

const char *ball = "B";

void panic(char *s) {
  fprintf(2, "%s\n", s);
  exit(1);
}

void pingpong(int infd, int outfd, char processCode) {
  char c;
  int count = 0;
  if (processCode == 'A') write(outfd, ball, 1);

  while(read(infd, &c, 1) > 0) {
    if(processCode == 'A') fprintf(1, "A to B");
    else fprintf(1, "B to A");
    if(++count > 1000) break;
    write(outfd, ball, 1);
  }

  close(infd);
  close(outfd);
  exit(0);
}

int main(int argc, char *argv[]) {
  int p1[2];
  int p2[2];
  int starttime;
  int endtime;

  pipe(p1);
  pipe(p2);

  // child A
  starttime = uptime();
  if(fork()  == 0) {
    close(p1[1]);
    close(p2[0]);
    pingpong(p1[0], p2[1], 'A');
  }

  // child B
  if(fork() == 0) {
    close(p2[1]);
    close(p1[0]);
    pingpong(p2[0], p1[1], 'B');
  }

  close(p1[0]);
  close(p1[1]);
  close(p2[0]);
  close(p2[1]);

  // wait for child process to finish
  wait(0);
  wait(0);
  endtime = uptime();
  printf("used %d seconds for exchange a byte between two process", endtime - starttime);
  exit(0);
}