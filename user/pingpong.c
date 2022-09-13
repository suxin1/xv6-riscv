// Transfer a byte between two process throw pipe;
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

void pingpong(int read_fd, int write_fd, char processCode) {
  char c;
  int count = 0;
  if (processCode == 'A') write(write_fd, ball, 1); // start then ping-pong game

  // 循环从 read_fd 读取一个字符到变量 c。
  while(read(read_fd, &c, 1) > 0) {
    fprintf(1, "round: %d\n", count);
    if(processCode == 'A') fprintf(1, "A to B\n");
    else fprintf(1, "B to A\n");
    if(++count > 1000) break;
    write(write_fd, ball, 1);
  }

  close(read_fd);
  close(write_fd);
  exit(0);
}

int main(int argc, char *argv[]) {
  // save pipe file descriptor
  int p1[2];
  int p2[2];

  int starttime;
  int endtime;

  // pipe 创建一个小内核缓存，并暴露给进程使用，允许两个进程之间相互通讯。
  // pipe[0] 为读取端，pipe[1] 为写入端，读取和写入端完全没有进程引用时会被关闭。
  // 例：close(pipe[0]) 释放当前进程 pipe[0] 的引用。
  // read 会阻断程序运行直到 不可能再读取数据，比如 pipe[1](write) 文件描述器不在被任何进程引用。
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
    close(p1[0]);
    close(p2[1]);
    pingpong(p2[0], p1[1], 'B');
  }

  // 主进程中释放所有 创建的 文件描述器
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