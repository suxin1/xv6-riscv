#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

char *
strcpy(char *s, const char *t) {
  char *os;

  os = s;
  while ((*s++ = *t++) != 0);
  return os;
}

int
strcmp(const char *p, const char *q) {
  while (*p && *p == *q)
    p++, q++;
  return (uchar) * p - (uchar) * q;
}

uint
strlen(const char *s) {
  int n;

  for (n = 0; s[n]; n++);
  return n;
}

void *
memset(void *dst, int c, uint n) {
  char *cdst = (char *) dst;
  int i;
  for (i = 0; i < n; i++) {
    cdst[i] = c;
  }
  return dst;
}

char *
strchr(const char *s, char c) {
  for (; *s; s++)
    if (*s == c)
      return (char *) s;
  return 0;
}

/**
 * 从标准输入（standard input）读取字符数据
 * 遇到 \n || \r 或 read 读取数量为 0 时停止读取
 * @param buf : 字符缓冲区
 * @param max : 读取字符数量
 * @return char * : 以 \0 结尾
 */
char *gets(char *buf, int max) {
  int i, cc;
  char c;

  for (i = 0; i + 1 < max;) {
    // 这里硬件会反复转换应用程序权限，可能会比较耗时
    // 从 standard input 复制1个字节（byte）数据到 c 并返回读取数量
    // 每个文件描述器关联一个 offset 属性， 每次 read 从 offset 开始读取数据并将读取到的字符数累加到 offset
    cc = read(0, &c, 1);
    if (cc < 1)
      break;
    buf[i++] = c;
    if (c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0'; // 字符串结束符号
  return buf;
}

int
stat(const char *n, struct stat *st) {
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if (fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

int
atoi(const char *s) {
  int n;

  n = 0;
  while ('0' <= *s && *s <= '9')
    n = n * 10 + *s++ - '0';
  return n;
}

void *
memmove(void *vdst, const void *vsrc, int n) {
  char *dst;
  const char *src;

  dst = vdst;
  src = vsrc;
  if (src > dst) {
    while (n-- > 0)
      *dst++ = *src++;
  } else {
    dst += n;
    src += n;
    while (n-- > 0)
      *--dst = *--src;
  }
  return vdst;
}

int
memcmp(const void *s1, const void *s2, uint n) {
  const char *p1 = s1, *p2 = s2;
  while (n-- > 0) {
    if (*p1 != *p2) {
      return *p1 - *p2;
    }
    p1++;
    p2++;
  }
  return 0;
}

void *
memcpy(void *dst, const void *src, uint n) {
  return memmove(dst, src, n);
}
