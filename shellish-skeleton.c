#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h> // termios, TCSANOW, ECHO, ICANON
#include <unistd.h>
#include <fcntl.h>

const char *sysname = "shellish";

enum return_codes {
  SUCCESS = 0,
  EXIT = 1,
  UNKNOWN = 2,
};

struct command_t {
  char *name;
  bool background;
  bool auto_complete;
  int arg_count;
  char **args;
  char *redirects[3];     // in/out redirection
  struct command_t *next; // for piping
};

/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command) {
  int i = 0;
  printf("Command: <%s>\n", command->name);
  printf("\tIs Background: %s\n", command->background ? "yes" : "no");
  printf("\tNeeds Auto-complete: %s\n", command->auto_complete ? "yes" : "no");
  printf("\tRedirects:\n");
  for (i = 0; i < 3; i++)
    printf("\t\t%d: %s\n", i,
           command->redirects[i] ? command->redirects[i] : "N/A");
  printf("\tArguments (%d):\n", command->arg_count);
  for (i = 0; i < command->arg_count; ++i)
    printf("\t\tArg %d: %s\n", i, command->args[i]);
  if (command->next) {
    printf("\tPiped to:\n");
    print_command(command->next);
  }
}

/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command) {
  if (command->arg_count) {
    for (int i = 0; i < command->arg_count; ++i)
      free(command->args[i]);
    free(command->args);
  }
  for (int i = 0; i < 3; ++i)
    if (command->redirects[i])
      free(command->redirects[i]);
  if (command->next) {
    free_command(command->next);
    command->next = NULL;
  }
  free(command->name);
  free(command);
  return 0;
}

/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt() {
  char cwd[1024], hostname[1024];
  gethostname(hostname, sizeof(hostname));
  getcwd(cwd, sizeof(cwd));
  printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
  return 0;
}

/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command) {
  const char *splitters = " \t"; // split at whitespace
  int index, len;
  len = strlen(buf);
  while (len > 0 && strchr(splitters, buf[0]) != NULL) // trim left whitespace
  {
    buf++;
    len--;
  }
  while (len > 0 && strchr(splitters, buf[len - 1]) != NULL)
    buf[--len] = 0; // trim right whitespace

  if (len > 0 && buf[len - 1] == '?') // auto-complete
    command->auto_complete = true;
  if (len > 0 && buf[len - 1] == '&') // background
    command->background = true;

  char *pch = strtok(buf, splitters);
  if (pch == NULL) {
    command->name = (char *)malloc(1);
    command->name[0] = 0;
  } else {
    command->name = (char *)malloc(strlen(pch) + 1);
    strcpy(command->name, pch);
  }

  command->args = (char **)malloc(sizeof(char *));

  int redirect_index;
  int arg_index = 0;
  char temp_buf[1024], *arg;
  while (1) {
    // tokenize input on splitters
    pch = strtok(NULL, splitters);
    if (!pch)
      break;
    arg = temp_buf;
    strcpy(arg, pch);
    len = strlen(arg);

    if (len == 0)
      continue; // empty arg, go for next
    while (len > 0 && strchr(splitters, arg[0]) != NULL) // trim left whitespace
    {
      arg++;
      len--;
    }
    while (len > 0 && strchr(splitters, arg[len - 1]) != NULL)
      arg[--len] = 0; // trim right whitespace
    if (len == 0)
      continue; // empty arg, go for next

    // piping to another command
    if (strcmp(arg, "|") == 0) {
      struct command_t *c =
          (struct command_t *)malloc(sizeof(struct command_t));
	  memset(c, 0, sizeof(struct command_t));
      int l = strlen(pch);
      pch[l] = splitters[0]; // restore strtok termination
      index = 1;
      while (pch[index] == ' ' || pch[index] == '\t')
        index++; // skip whitespaces

      parse_command(pch + index, c);
      pch[l] = 0; // put back strtok termination
      command->next = c;
      continue;
    }

    // background process
    if (strcmp(arg, "&") == 0)
      continue; // handled before

    // handle input redirection
    redirect_index = -1;
    if (arg[0] == '<')
      redirect_index = 0;
    if (arg[0] == '>') {
      if (len > 1 && arg[1] == '>') {
        redirect_index = 2;
        arg++;
        len--;
      } else
        redirect_index = 1;
    }
    if (redirect_index != -1) {
      command->redirects[redirect_index] = (char *)malloc(len+1);
      strcpy(command->redirects[redirect_index], arg + 1);
      continue;
    }

    // normal arguments
    if (len > 2 &&
        ((arg[0] == '"' && arg[len - 1] == '"') ||
         (arg[0] == '\'' && arg[len - 1] == '\''))) // quote wrapped arg
    {
      arg[--len] = 0;
      arg++;
    }
    command->args =
        (char **)realloc(command->args, sizeof(char *) * (arg_index + 1));
    command->args[arg_index] = (char *)malloc(len + 1);
    strcpy(command->args[arg_index++], arg);
  }
  command->arg_count = arg_index;

  // increase args size by 2
  command->args = (char **)realloc(command->args,
                                   sizeof(char *) * (command->arg_count += 2));

  // shift everything forward by 1
  for (int i = command->arg_count - 2; i > 0; --i)
    command->args[i] = command->args[i - 1];

  // set args[0] as a copy of name
  command->args[0] = strdup(command->name);
  // set args[arg_count-1] (last) to NULL
  command->args[command->arg_count - 1] = NULL;

  return 0;
}

void prompt_backspace() {
  putchar(8);   // go back 1
  putchar(' '); // write empty over
  putchar(8);   // go back 1 again
}

/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command) {
  int index = 0;
  char c;
  char buf[4096];
  static char oldbuf[4096];

  // tcgetattr gets the parameters of the current terminal
  // STDIN_FILENO will tell tcgetattr that it should write the settings
  // of stdin to oldt
  static struct termios backup_termios, new_termios;
  tcgetattr(STDIN_FILENO, &backup_termios);
  new_termios = backup_termios;
  // ICANON normally takes care that one line at a time will be processed
  // that means it will return if it sees a "\n" or an EOF or an EOL
  new_termios.c_lflag &=
      ~(ICANON |
        ECHO); // Also disable automatic echo. We manually echo each char.
  // Those new settings will be set to STDIN
  // TCSANOW tells tcsetattr to change attributes immediately.
  tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

  show_prompt();
  buf[0] = 0;
  while (1) {
    c = getchar();
    // printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

    if (c == 9) // handle tab
    {
      buf[index++] = '?'; // autocomplete
      break;
    }

    if (c == 127) // handle backspace
    {
      if (index > 0) {
        prompt_backspace();
        index--;
      }
      continue;
    }

    if (c == 27 || c == 91 || c == 66 || c == 67 || c == 68) {
      continue;
    }

    if (c == 65) // up arrow
    {
      while (index > 0) {
        prompt_backspace();
        index--;
      }

      char tmpbuf[4096];
      printf("%s", oldbuf);
      strcpy(tmpbuf, buf);
      strcpy(buf, oldbuf);
      strcpy(oldbuf, tmpbuf);
      index += strlen(buf);
      continue;
    }

    putchar(c); // echo the character
    buf[index++] = c;
    if (index >= sizeof(buf) - 1)
      break;
    if (c == '\n') // enter key
      break;
    if (c == 4) // Ctrl+D
      return EXIT;
  }
  if (index > 0 && buf[index - 1] == '\n') // trim newline from the end
    index--;
  buf[index++] = '\0'; // null terminate string

  strcpy(oldbuf, buf);

  parse_command(buf, command);

  // print_command(command); // DEBUG: uncomment for debugging

  // restore the old settings
  tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
  return SUCCESS;
}


//  PART3(a): builtin cut

static int parse_fields(const char *s, int **out_fields, int *out_n) {
  // s: "1,3,10"
  int cap = 8, n = 0;
  int *arr = malloc(sizeof(int) * cap);
  if (!arr) return -1;

  const char *p = s;
  while (*p) {
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) break;

    char *endptr = NULL;
    long v = strtol(p, &endptr, 10);
    if (endptr == p) { // not a number
      free(arr);
      return -1;
    }
    if (v <= 0) {
      free(arr);
      return -1;
    }
    if (n >= cap) {
      cap *= 2;
      int *tmp = realloc(arr, sizeof(int) * cap);
      if (!tmp) { free(arr); return -1; }
      arr = tmp;
    }
    arr[n++] = (int)v;

    p = endptr;
    if (*p == ',') p++;
    else if (*p) {
      // allow trailing spaces, otherwise invalid
      while (*p == ' ' || *p == '\t') p++;
      if (*p == ',') p++;
      else if (*p != '\0') { free(arr); return -1; }
    }
  }

  if (n == 0) { free(arr); return -1; }
  *out_fields = arr;
  *out_n = n;
  return 0;
}

static void split_preserve_empty(char *line, char delim, char ***out_parts, int *out_cnt) {
  // line is mutable; we split in-place but preserve empty fields
  int cap = 16, cnt = 0;
  char **parts = malloc(sizeof(char*) * cap);
  if (!parts) { *out_parts = NULL; *out_cnt = 0; return; }

  char *start = line;
  for (char *p = line; ; p++) {
    if (*p == delim || *p == '\n' || *p == '\0') {
      if (cnt >= cap) {
        cap *= 2;
        char **tmp = realloc(parts, sizeof(char*) * cap);
        if (!tmp) break;
        parts = tmp;
      }
      parts[cnt++] = start;

      if (*p == '\n' || *p == '\0') {
        *p = '\0';
        break;
      }
      *p = '\0';
      start = p + 1;
    }
  }

  *out_parts = parts;
  *out_cnt = cnt;
}

static int builtin_cut(struct command_t *command) {
  // Supported:
  // -d X / --delimiter X   (single char)
  // -f list / --fields list  (comma-separated 1-based indices)
  char delim = '\t';
  int *fields = NULL, nfields = 0;

  // args: command->args[0] = "cut"
  // real args are [1 .. arg_count-2]
  for (int i = 1; i < command->arg_count - 1; i++) {
    char *a = command->args[i];
    if (!a) break;

    if (strcmp(a, "-d") == 0 || strcmp(a, "--delimiter") == 0) {
      if (i + 1 >= command->arg_count - 1) {
        fprintf(stderr, "cut: missing delimiter after %s\n", a);
        return 1;
      }
      char *d = command->args[++i];
      if (!d || strlen(d) != 1) {
        fprintf(stderr, "cut: delimiter must be a single character\n");
        return 1;
      }
      delim = d[0];
    } else if (strcmp(a, "-f") == 0 || strcmp(a, "--fields") == 0) {
      if (i + 1 >= command->arg_count - 1) {
        fprintf(stderr, "cut: missing fields list after %s\n", a);
        return 1;
      }
      char *flist = command->args[++i];
      if (parse_fields(flist, &fields, &nfields) != 0) {
        fprintf(stderr, "cut: invalid fields list: %s\n", flist);
        return 1;
      }
    } else {
      // ignore unknown options or treat as error 
      fprintf(stderr, "cut: unknown option: %s\n", a);
      free(fields);
      return 1;
    }
  }

  if (!fields) {
    fprintf(stderr, "cut: fields are required. Use -f 1,3,...\n");
    return 1;
  }

  char *line = NULL;
  size_t cap = 0;

  while (1) {
    ssize_t r = getline(&line, &cap, stdin);
    if (r ==-1) break;

    // split line into fields (preserve empty)
    char **parts = NULL;
    int nparts = 0;
    split_preserve_empty(line, delim, &parts, &nparts);

    for (int k = 0; k < nfields; k++) {
      int idx = fields[k]; // 1-based
      if (k > 0) putchar(delim);

      if (idx >= 1 && idx <= nparts) {
        fputs(parts[idx - 1], stdout);
      }
    }
    putchar('\n');

    free(parts);
  }

  free(fields);
  free(line);
  return 0;
}

// PATH resolve + execv for external commands
static void exec_external(struct command_t *cmd) {
  char *path = getenv("PATH");
  if (!path) path = "";

  char *copy = strdup(path);
  if (!copy) exit(1);

  char *dir = strtok(copy, ":");
  while (dir != NULL) {
    char fullpath[1024];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, cmd->name);

    if (access(fullpath, X_OK) == 0) {
      execv(fullpath, cmd->args);
    }
    dir = strtok(NULL, ":");
  }
  free(copy);

  printf("-%s: %s: command not found\n", sysname, cmd->name);
  exit(127);
}

// run in child: builtin or external?
static void run_command_child(struct command_t *cmd) {
  if (strcmp(cmd->name, "cut") == 0) {
    int rc = builtin_cut(cmd);
    exit(rc);
  }
  exec_external(cmd);
}

int process_command(struct command_t *command) {
    int r;
    if (strcmp(command->name, "") == 0)
      return SUCCESS;

    if (strcmp(command->name, "exit") == 0)
      return EXIT;

    if (strcmp(command->name, "cd") == 0) {
      if (command->arg_count > 0) {
        r = chdir(command->args[1]);
        if (r == -1)
          printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
      }
      return SUCCESS;
    }
	// PART2 Pipe Case
    if (command->next != NULL) {
    int fd[2];
    if (pipe(fd) < 0) {
      printf("-%s: pipe failed: %s\n", sysname, strerror(errno));
      return SUCCESS;
    }


    pid_t p1 = fork();
    if (p1 == 0) {
      // child 1 stdout pipe wrİte end
     if (dup2(fd[1], STDOUT_FILENO) < 0) exit(1);
      close(fd[0]);
      close(fd[1]);

      //  for child1 just input redirection (<)
      if (command->redirects[0]) {
        int fd_in = open(command->redirects[0], O_RDONLY);
        if (fd_in < 0) {
          printf("-%s: cannot open input file %s: %s\n",
                 sysname, command->redirects[0], strerror(errno));
          exit(1);
        }
        if (dup2(fd_in, STDIN_FILENO) < 0) exit(1);
        close(fd_in);
      }
      run_command_child(command);
     }

    pid_t p2 = fork();
    if (p2 == 0) {
      // child 2: stdin <- pipe read end
      if (dup2(fd[0], STDIN_FILENO) < 0) exit(1);
      close(fd[1]);
      close(fd[0]);

      // for child2 output redirection (> / >>) cmd1 | cmd2 >out
      if (command->next->redirects[1]) {
        int fd_out = open(command->next->redirects[1],
                          O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd_out < 0) {
          printf("-%s: cannot open output file %s: %s\n",
                 sysname, command->next->redirects[1], strerror(errno));
          exit(1);
        }
        if (dup2(fd_out, STDOUT_FILENO) < 0) exit(1);
        close(fd_out);
      }


      if (command->next->redirects[2]) {
        int fd_app = open(command->next->redirects[2],
                          O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd_app < 0) {
          printf("-%s: cannot open append file %s: %s\n",
                 sysname, command->next->redirects[2], strerror(errno));
          exit(1);
        }
        if (dup2(fd_app, STDOUT_FILENO) < 0) exit(1);
        close(fd_app);
      }
      run_command_child(command->next);
     }

    close(fd[0]);
    close(fd[1]);

    if (!command->background) {
      waitpid(p1, NULL, 0);
      waitpid(p2, NULL, 0);
    }

    return SUCCESS;
}

  pid_t pid = fork();
  if (pid == 0) // child
  {
    //PART2 I/O Redirection
    // redirects[0] : input  (<file)
    // redirects[1] : output (>file) truncate
    // redirects[2] : output (>>file) append

   if (command->redirects[0]) {
      int fd_in = open(command->redirects[0], O_RDONLY);
      if (fd_in < 0) {
        printf("-%s: cannot open input file %s: %s\n",
               sysname, command->redirects[0], strerror(errno));
        exit(1);
      }
      if (dup2(fd_in, STDIN_FILENO) < 0) exit(1);
      close(fd_in);

    }


    //if > and >> together, last one will win so first > then >> 
    if (command->redirects[1]) {
      int fd_out = open(command->redirects[1],
                        O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd_out < 0) {
        printf("-%s: cannot open output file %s: %s\n",
               sysname, command->redirects[1], strerror(errno));
        exit(1);
      }
      if (dup2(fd_out, STDOUT_FILENO) < 0)  exit(1);
     close(fd_out);
    }

    if (command->redirects[2]) {
      int fd_app = open(command->redirects[2],
                        O_WRONLY | O_CREAT | O_APPEND, 0644);
      if (fd_app < 0) {
        printf("-%s: cannot open append file %s: %s\n",
               sysname, command->redirects[2], strerror(errno));
        exit(1);
      }
      if (dup2(fd_app, STDOUT_FILENO) < 0) exit(1);
      close(fd_app);
    }
    run_command_child(command);
    exit(1);
    }
    else{
	if(!command->background){
	   waitpid(pid,NULL,0);
	}
	return SUCCESS;
    }
}
    //end redirecion


    /// This shows how to do exec with environ (but is not available on MacOs)
    // extern char** environ; // environment variables
    // execvpe(command->name, command->args, environ); // exec+args+path+environ

    /// This shows how to do exec with auto-path resolve
    // add a NULL argument to the end of args, and the name to the beginning
    // as required by exec

    // TODO: do your own exec with path resolving using execv()
    // do so by replacing the execvp call below
    // exec+args+path

int main() {
  while (1) {
    struct command_t *command =
        (struct command_t *)malloc(sizeof(struct command_t));
    memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

    int code;
    code = prompt(command);
    if (code == EXIT){
     free_commanmd(command);
     break;
    }

    code = process_command(command);
    if (code == EXIT)
      break;

    free_command(command);
  }

  printf("\n");
  return 0;
}
