#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define MAXARGS 256

// All commands have at least a type. Have looked at the type, the code
// typically casts the *cmd to some specific cmd type.
struct cmd {
  int type;          //  ' ' (exec), | (pipe), '<' or '>' for redirection
};

struct execcmd {
  int type;              // ' '
  char *argv[MAXARGS];   // arguments to the command to be exec-ed
};

struct redircmd {
  int type;          // < or > 
  struct cmd *cmd;   // the command to be run (e.g., an execcmd)
  char *file;        // the input/output file
  int mode;          // the mode to open the file with
  int fd;            // the file descriptor number to use for the file
};

struct pipecmd {
  int type;          // |
  struct cmd *left;  // left side of pipe
  struct cmd *right; // right side of pipe
};

int fork1(void);  // Fork but exits on failure.
struct cmd *parsecmd(char*);

void runcmd(struct cmd *cmd)
{
  int p[2], r;
  struct execcmd *ecmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit(0);
  
  switch(cmd->type){
  default:
    fprintf(stderr, "unknown runcmd\n");
    exit(-1);

  case ' ':
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit(0);
    // built-in commands
    // // buggy
    // if (strcmp(ecmd->argv[0], "cd1") == 0) {
    //   chdir(ecmd->argv[1]);
    // }
    // if (strcmp(ecmd->argv[0], "exit") == 0) {
    //   exit(0);
    // }
    // // buggy over
    if (strcmp(ecmd->argv[0], "pwd") == 0) {
      char wd[4096];
      puts(getcwd(wd, 4096));
      break;
    }
    if (strcmp(ecmd->argv[0], "export") == 0){
      // char key[256];
      // char value[256];
      char s[256];
      strcpy(s, ecmd->argv[1]);
      printf("%s", s);
      char* div = strchr(s, '='); // 找到等号位置
      if (div != NULL){
        putenv(s);
      }
      // else
      // *div = '\0';
      // strcpy(key, s); // 获取 key 值
      // strcpy(value, div + 1); // 获取 value 值
    }
    // external commands
    execvp(ecmd->argv[0], ecmd->argv);
    break;

  case '>':
  case '<':
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0)
    {
        fprintf(stderr, "Open file :%s failed\n", rcmd->file);
        exit(0);
    }
    runcmd(rcmd->cmd);
    break;

  case '|':
    pcmd = (struct pipecmd*)cmd;
    if (pipe(p) < 0)  // 创建管道
      fprintf(stderr, "create pipe failure!\n");
    if (fork1() == 0){
      close(1);
      // dup(p[1]);
      dup2(p[1], 1);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd -> left);
    }
    if (fork1() == 0){
      close(0);
      // dup(p[0]);
      dup2(p[0], 0);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd -> right);
    }
    close(p[0]);
    close(p[1]);
    wait(NULL);
    wait(NULL);
    break;
  }    
  exit(0);
}

int getcmd(char *buf, int nbuf) // 获取命令
{
  
  if (isatty(fileno(stdin)))
    fprintf(stdout, "$ ");
  memset(buf, 0, nbuf);
  // char t;
  // t = getchar();
  // if(t == '\n')
  //   return -1;
  // fgets(buf+1, nbuf, stdin);
  fgets(buf, nbuf, stdin);
  // buf[0] = t;
  return 0;
}

int main() {
    /* 输入缓冲区 */
    static char buf[256];
    struct cmd* usercmd;
    struct execcmd* globalcmd;
    while (1) {
      // 获取命令 
      // scanf("%*[^\n]%*c"); // 清空 stdin
      if(getcmd(buf, sizeof(buf)) < 0)
        continue;
      if(buf[0] == '\n')
        continue;
      if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
        // 特判处理 Chdir 命令
        // 如果用子进程执行 cd 命令，那么父进程工作目录不会改变
        // 因此要在 fork 之前处理
        buf[strlen(buf)-1] = 0;  // 把 \n 换成 \0
        if(chdir(buf+3) < 0)
          fprintf(stderr, "cannot cd %s\n", buf+3);
        continue;
      }
      usercmd = parsecmd(buf);
      if (usercmd->type == ' '){
        // 原理和 Chdir 类似
        globalcmd = (struct execcmd*) usercmd;
        if (strcmp(globalcmd->argv[0], "export") == 0){
          putenv(globalcmd->argv[1]);
          continue;
        }
        if (strcmp(globalcmd->argv[0], "exit") == 0) {
          fflush(stdout);
          exit(0);
        }
      }
      if(fork1() == 0)
        // 子进程创建成功，开始执行终端命令
        runcmd(usercmd);
      /* 父进程 */
      wait(NULL);
    }
}

pid_t fork1(void)
{
  pid_t pid;
  
  pid = fork();
  if(pid == -1)
    perror("fork");
  return pid;
}

struct cmd* execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = ' ';
  return (struct cmd*)cmd;
}

struct cmd* redircmd(struct cmd *subcmd, char *file, int type)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = type;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->mode = (type == '<') ?  O_RDONLY : O_WRONLY|O_CREAT|O_TRUNC;
  cmd->fd = (type == '<') ? 0 : 1;
  return (struct cmd*)cmd;
}

struct cmd* pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = '|';
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>";

int gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;
  
  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '<':
    s++;
    break;
  case '>':
    s++;
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;
  
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int peek(char **ps, char *es, char *toks)
{
  char *s;
  
  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);

// make a copy of the characters in the input buffer, starting from s through es.
// null-terminate the copy to make it a string.
char 
*mkcopy(char *s, char *es)
{
  int n = es - s;
  char *c = malloc(n+1);
  assert(c);
  strncpy(c, s, n);
  c[n] = 0;
  return c;
}

struct cmd* parsecmd(char *s)
{
  char *es; // 命令结束处
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(stderr, "leftovers: %s\n", s);
    exit(-1);
  }
  return cmd;
}

struct cmd*parseline(char **ps, char *es)
{
  struct cmd *cmd;
  cmd = parsepipe(ps, es);
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a') {
      fprintf(stderr, "missing file for redirection\n");
      exit(-1);
    }
    switch(tok){
    case '<':
      cmd = redircmd(cmd, mkcopy(q, eq), '<');
      break;
    case '>':
      cmd = redircmd(cmd, mkcopy(q, eq), '>');
      break;
    }
  }
  return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;
  
  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a') {
      fprintf(stderr, "syntax error\n");
      exit(-1);
    }
    cmd->argv[argc] = mkcopy(q, eq);
    argc++;
    if(argc >= MAXARGS) {
      fprintf(stderr, "too many args\n");
      exit(-1);
    }
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  return ret;
}
// fflush(stdout)