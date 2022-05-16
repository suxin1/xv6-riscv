// Shell.
// this program will be executed when return button was hit.

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10

struct cmd {
    int type;
};

struct execcmd {
    int type;
    char *argv[MAXARGS];
    char *eargv[MAXARGS];
};

struct redircmd {
    int type;
    struct cmd *cmd;
    char *file;
    char *efile;
    int mode;
    int fd;
    int sfd;
};

struct pipecmd {
    int type;
    struct cmd *left;
    struct cmd *right;
};

struct listcmd {
    int type;
    struct cmd *left;
    struct cmd *right;
};

struct backcmd {
    int type;
    struct cmd *cmd;
};

int fork1(void);  // Fork but panics on failure.
void panic(char *);

struct cmd *parsecmd(char *);

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd) {
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if (cmd == 0)
    exit(1);

  switch (cmd->type) {
    default:
      panic("runcmd");

    case EXEC:
      ecmd = (struct execcmd *) cmd;
      if (ecmd->argv[0] == 0)
        exit(1);
      exec(ecmd->argv[0], ecmd->argv);
      fprintf(2, "exec %s failed\n", ecmd->argv[0]);
      break;

    case REDIR:
      rcmd = (struct redircmd *) cmd;
      close(rcmd->fd);
      if (open(rcmd->file, rcmd->mode) < 0) {
        fprintf(2, "open %s failed\n", rcmd->file);
        exit(1);
      }
      runcmd(rcmd->cmd);
      break;

    case LIST:
      lcmd = (struct listcmd *) cmd;
      if (fork1() == 0) {
        runcmd(lcmd->left);
      }
      wait(0);
      runcmd(lcmd->right);
      break;

    case PIPE:
      pcmd = (struct pipecmd *) cmd;
      if (pipe(p) < 0)
        panic("pipe");
      if (fork1() == 0) {
        close(1);
        dup(p[1]);
        close(p[0]);
        close(p[1]);
        runcmd(pcmd->left);
      }
      if (fork1() == 0) {
        close(0);
        dup(p[0]);
        close(p[0]);
        close(p[1]);
        runcmd(pcmd->right);
      }
      close(p[0]);
      close(p[1]);
      wait(0);
      wait(0);
      break;

    case BACK:
      bcmd = (struct backcmd *) cmd;
      if (fork1() == 0)
        runcmd(bcmd->cmd);
      break;
  }
  exit(0);
}

/**
 * 从标准输入获取字符数据
 * @param buf 暂存区
 * @param nbuf 暂存区尺寸（大小）单位: byte(8bit)
 * @return int
 */
int getcmd(char *buf, int nbuf) {
  fprintf(2, "$ ");
  memset(buf, 0, nbuf);  // 将 buf 重置为0
  gets(buf, nbuf);
  if (buf[0] == 0) // EOF
    return -1;
  return 0;
}

int main(void) {
  static char buf[100];
  int fd;

  // Ensure that three file descriptors are open.
  while ((fd = open("console", O_RDWR)) >= 0) {
    if (fd >= 3) {
      close(fd);
      break;
    }
  }

  // Read and run input commands.
  while (getcmd(buf, sizeof(buf)) >= 0) {
    if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ') {
      // Chdir must be called by the parent, not the child.
      buf[strlen(buf) - 1] = 0;  // chop \n
      if (chdir(buf + 3) < 0)
        fprintf(2, "cannot cd %s\n", buf + 3);
      continue;
    }
    if (fork1() == 0)
      runcmd(parsecmd(buf));
    wait(0);
  }
  exit(0);
}

void panic(char *s) {
  fprintf(2, "%s\n", s);
  exit(1);
}

int fork1(void) {
  int pid;

  pid = fork();
  if (pid == -1)
    panic("fork");
  return pid;
}

//PAGEBREAK!
// Constructors

struct cmd *
execcmd(void) {
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd *) cmd;
}

struct cmd *redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd) {
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd *) cmd;
}

struct cmd *
pipecmd(struct cmd *left, struct cmd *right) {
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd *) cmd;
}

struct cmd *
listcmd(struct cmd *left, struct cmd *right) {
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd *) cmd;
}


struct cmd *backcmd(struct cmd *subcmd) {
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd *) cmd;
}
//PAGEBREAK!
// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";
char descriptors[] = "012";


/**
 * 取得 token: |();&<>
 * （不包含 |();&<>和空白字符 的字符串也表示一个token, 通过 q 和 eq 指针返回），
 * 并偏移 char **ps 到 token 后面的位置
 * 例：
 *   "| cat": 返回 "|", *q 指向 "|", *eq 指向 *q + 1, *ps 指向 "c";
 *   ">> filename": 返回 "+", *q 指向 ">", *eq 指向 *q + 2, *ps 指向 "f";
 *   " filename": 返回 "a", *q 指向 "f", *eq 指向 *q + 8, *ps 指向 *q + 8;
 *   ""：返回 0，结束字符
 *   TODO "2>&1": implement
 */
int gettoken(char **ps, char *es, char **q, char **eq) {
  char *s;
  int ret;

  s = *ps;
  while (s < es && strchr(whitespace, *s)) // 跳过空白字符
    s++;
  if (q)
    *q = s;
  ret = *s;
  switch (*s) {
    case 0: // 字符串结束
      break;
    case '|':
    case '(':
    case ')':
    case ';':
    case '&':
    case '<':
      s++;
      break;
    case '>':
      s++;
      if (*s == '>') {
        ret = '+';
        s++;
      }
      break;
    default:
      ret = 'a';
      while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
        s++;
      break;
  }
  if (eq)
    *eq = s;

  while (s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}


/**
 * 忽略掉 char **ps 所表示字符串里前面的空白字符
 * 返回 toks 是否包含 **ps 里面的第一个非空白字符
 */
int peek(char **ps, char *es, char *toks) {
  // ps 使用双指针是为了能够改变入参字串的起始位置（指针操作）
  char *s;

  s = *ps; // 第一个字符串的指针
  while (s < es && strchr(whitespace, *s)) // 跳过空白字符（空格，回车，tab, ...)
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char **, char *);

struct cmd *parsepipe(char **, char *);

struct cmd *parseexec(char **, char *);

struct cmd *nulterminate(struct cmd *);

struct cmd *parsecmd(char *s) {
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if (s != es) {
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

/**
 * 命令列表（顺序执行命令，前后无关联）
 * cmd
 *   |- type: LIST
 *   |- cmd: left
 *      |- type: BACK
 *      |- cmd: cmd
 *         |- type: EXEC | PIPE | REDIR
 *   |- cmd: right
 *      |- type: EXEC
 *
 * 返回 struct cmd 类型: LIST | BACK | PIPE | REDIR | EXEC
 */
struct cmd *parseline(char **ps, char *es) {
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while (peek(ps, es, "&")) {
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);  // 后台执行命令
  }
  if (peek(ps, es, ";")) {
    gettoken(ps, es, 0, 0); // 顺序执行命令，前后没有关联
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

/**
 * 管道命令
 * 最终返回的数据结构可能像下面这样：
 * cmd
 *   |- type: PIPE
 *   |- cmd: left
 *      |- type: REDIR
 *      |- cmd: cmd
 *         |- type: EXEC
 *   |- cmd: right
 *      |- type: EXEC
 *
 * cmd | cmd | cmd
 * L   | R        |   pipecmd1
 *     | L   | R  |   pipecmd1 -> right
 *
 * 返回 struct cmd 类型: REDIR | EXEC | PIPE
 */
struct cmd *parsepipe(char **ps, char *es) {
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if (peek(ps, es, "|")) {
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

/**
 * 如果字符串非空白起始字符包含重定向标记 '<' 或 '>'
 * 则返回一个被强制转换为 cmd 的 redircmd，并将 *ps 指针指向剩余未处理字符的第一个字符
 * 重定向规则与语法:
 *  command > file	将输出重定向到 file
 *  command < file	将输入重定向到 file
 *  command >> file	将输出以追加的方式重定向到 file
 *  n > file	将文件描述符为 n 的文件重定向到 file
 *  n >> file	将文件描述符为 n 的文件以追加的方式重定向到 file
 *  n >& m	将输出文件 m 和 n 合并
 *  n <& m	将输入文件 m 和 n 合并
 *  << tag	将开始标记 tag 和结束标记 tag 之间的内容作为输入
 */
struct cmd *parseredirs(struct cmd *cmd, char **ps, char *es) {
  int tok;
  char *q, *eq;  // q: 文件字符串起始指针，eq: 文件字符串结束指针

  while (peek(ps, es, "<>")) {
    tok = gettoken(ps, es, 0, 0);
    if (gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch (tok) {
      case '<':
        // 将文件重定向到标准输入
        cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
        break;
      case '>':
        // 将文件以可写，可创建，重定向到标准输出
        cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE | O_TRUNC, 1);
        break;
      case '+':  // >>
        // 将文件以可写，可创建重定向到标准输出
        cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE, 1);
        break;
      case 'd':
        int fd = atoi(*q);
//        if (gettoken(ps, es, &q, &eq) != 'a') {
//          panic("missing file descriptor for redirection");
//        }
    }
  }
  return cmd;
}

struct cmd *parseblock(char **ps, char *es) {
  struct cmd *cmd;

  if (!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if (!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

/**
 * 从字符串中解析出可执行命令，遇到 "|)&;" 时停止解析并返回一个 cmd 结构体
 * 最终返回的数据结构可能像下面这样：
 * cmd
 *   |- type: REDIR
 *   |- cmd
 *      |- type: REDIR
 *      |- cmd: cmd
 *         |- type: EXEC
 * type: 的作用是方便后续执行命令时将其转换为对应 命令 的 数据结构
 * 返回 struct cmd 类型: REDIR | EXEC
 */
struct cmd *parseexec(char **ps, char *es) {
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if (peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();  // 被强制转换为 struct cmd * 的 struct execcmd * 的指针
  cmd = (struct execcmd *) ret;

  argc = 0; // argument count

  // 被强制转换为 struct cmd * 的 struct redircmd * | struct execcmd * 的指针
  ret = parseredirs(ret, ps, es);
  while (!peek(ps, es, "|)&;")) {
    if ((tok = gettoken(ps, es, &q, &eq)) == 0)
      break;
    if (tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if (argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
// 将所有命令中包含的结尾字符赋值为0，使其变为一个有效的字符串
// ls filename1 filename20
// ls0filename10filename20
struct cmd *nulterminate(struct cmd *cmd) {
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if (cmd == 0)
    return 0;

  switch (cmd->type) {
    case EXEC:
      ecmd = (struct execcmd *) cmd;
      for (i = 0; ecmd->argv[i]; i++)
        *ecmd->eargv[i] = 0;
      break;

    case REDIR:
      rcmd = (struct redircmd *) cmd;
      nulterminate(rcmd->cmd);
      *rcmd->efile = 0;
      break;

    case PIPE:
      pcmd = (struct pipecmd *) cmd;
      nulterminate(pcmd->left);
      nulterminate(pcmd->right);
      break;

    case LIST:
      lcmd = (struct listcmd *) cmd;
      nulterminate(lcmd->left);
      nulterminate(lcmd->right);
      break;

    case BACK:
      bcmd = (struct backcmd *) cmd;
      nulterminate(bcmd->cmd);
      break;
  }
  return cmd;
}
