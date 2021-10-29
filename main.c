/*
 * A simple interactive command-line shell.
 * Written mostly as an experiment.
 * Copyright (c) 2021 Ocawesome101 under the DSLv2.
 * Compile with `-lreadline`.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <string.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>

#ifndef NO_DEBUG
#define DEBUG
#endif

#define MAX_WORDS 64
#define MAX_WORD_LENGTH 128

int exec_and_wait(int input, int output, const char *file, char *const argv[]) {
  int close_input = (input == STDIN_FILENO) ? 1 : 0;
  int close_output = (output == STDOUT_FILENO) ? 1 : 0;
  if (strcmp(file, "cd") == 0) {
    char* arg;
    if (argv[1] == NULL) {
      arg = getenv("HOME");
    } else {
      arg = argv[1];
    }
    chdir(arg);
    char* abuf = malloc(4096);
    getcwd(abuf, 4096);
    setenv("PWD", abuf, 1);
    free(abuf);
    return 0;
  } else if (strcmp(file, "exit") == 0) {
    exit(0);
  } else if (strcmp(file, "set") == 0) {
    if (argv[1] != NULL && argv[2] != NULL) {
      setenv(argv[1], argv[2], 1);
    } else {
      extern char **environ;
      int i;
      for (i = 0; environ[i] != NULL; i++) {
        write(output, environ[i], strlen(environ[i]));
        write(output, "\n", 1);
      }
      if (close_input == 0)
        close(input);
      if (close_output == 0)
        close(output);
    }
  } else {
    pid_t pid = fork();
    switch (pid) {
      case -1:
        break;
  
      case 0: /* Child */
        if (close_input == 0) {
          dup2(input, STDIN_FILENO);
          setvbuf(stdin, NULL, _IONBF, 0);
        }
        if (close_output == 0) {
          dup2(output, STDOUT_FILENO);
          setvbuf(stdout, NULL, _IONBF, 0);
        }
        execvp(file, argv);
        fprintf(stderr, "%s: command not found or could not be executed\n",
            file);
  
      default: /* Parent */
        int stat_loc;
        waitpid(pid, &stat_loc, 0);
        if (close_input == 0)
          close(input);
        if (close_output == 0)
          close(output);
        return stat_loc;
    }
  }
  return 0;
}

typedef struct command {
  int input;
  int output;
  char* argv[MAX_WORDS + 1];
} Command;

int exec_vect(char *const argv[]) {
  Command* commands[(MAX_WORDS + 1) * sizeof(int*)];
  int i = 0;
  int c = 0;
  int a = 0;
  int n = 0;
  Command* cmd = malloc(sizeof(Command));
  cmd->input = STDIN_FILENO;
  cmd->output = STDOUT_FILENO;
  cmd->argv[0] = NULL;
  commands[c] = cmd;
#ifdef DEBUG
  fprintf(stderr, "New struct pointer: %p\n", commands[c]);
#endif
  for (i = 0; argv[i] != NULL; i++) {
    if (strcmp(argv[i], "|") == 0) {
      commands[c]->argv[a] = NULL;
#ifdef DEBUG
      fprintf(stderr, "null-terminating command list %d at item %d\n", c, a);
#endif
      
      int pipes[2];
      pipe(pipes);

#ifdef DEBUG
      fprintf(stderr, "Created pipe %d -> %d\n", pipes[1], pipes[0]);
#endif
      
      commands[c]->output = pipes[1];
      c++; // heh
      a = 0;
      
      Command *cmd = malloc(sizeof(Command));
      commands[c] = cmd;
#ifdef DEBUG
      fprintf(stderr, "New struct pointer: %p\n", commands[c]);
      fprintf(stderr, "Assigned output of command %d to pipe %d, and input of command %d to pipe %d\n", c - 1, pipes[1], c, pipes[0]);
#endif
      cmd->input = pipes[0];
      cmd->output = STDOUT_FILENO;
      cmd->argv[0] = NULL;
    } else {
#ifdef DEBUG
      fprintf(stderr, "Adding '%s' to command %d\n", argv[i], c);
#endif
      commands[c]->argv[a] = malloc(MAX_WORD_LENGTH);
      strcpy(commands[c]->argv[a], argv[i]);
      a++;
    }
  }
#ifdef DEBUG
  fprintf(stderr, "null-terminating command list %d at item %d\n", c, a);
#endif
  commands[c]->argv[a] = NULL;
  c++;
  commands[c] = NULL;

  // now, execute all that
  for (i = 0; commands[i] != NULL; i++) {
#ifdef DEBUG
    fprintf(stderr, "check commands[%d]\n", i);
#endif
    if (commands[i]->argv[0] != NULL) {
#ifdef DEBUG
      fprintf(stderr, "exec command '%s' (%d) input %d output %d\n",
          commands[i]->argv[0], i, commands[i]->input, commands[i]->output);
#endif
      exec_and_wait(commands[i]->input, commands[i]->output,
          commands[i]->argv[0], commands[i]->argv);
#ifdef DEBUG
      fprintf(stderr, "finished - freeing\n");
#endif
      // clean up
      for (n = 0; commands[i]->argv[n] != NULL; n++) {
        free(commands[i]->argv[n]);
      }
      free(commands[i]);
    }
  }
  return 0;
}

int main() {
  char hostname[128];
  gethostname(hostname, 128);
  setenv("PROMPT", "$ ", 0);

  setenv("PATH",
      "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin", 0);

  while (1) {
    char* str = readline(getenv("PROMPT"));
    if (str == NULL) {
      exit(0);
    }

    int len = strlen(str);
    
    if (len > 0) {
      char* argv[MAX_WORDS + 1];
      int i = 0;
      int alen = 0;
      int word = 0;
      int instr = 0;
      argv[0] = malloc(MAX_WORD_LENGTH);
      for (i = 0; i < len; i++) {
        if (str[i] == ' ' && alen > 0 && (i + 1) < len && instr == 0) {
          argv[word][alen] = '\0';
          word += 1;
          alen = 0;
          argv[word] = malloc(MAX_WORD_LENGTH);
          
          if (word > MAX_WORDS) {
            fprintf(stderr, "Word limit of %d exceeded\n", MAX_WORDS);
            break;
          }
        } else if (alen > MAX_WORD_LENGTH) {
          fprintf(stderr, "Word length limit of %d exceeded\n",
              MAX_WORD_LENGTH);
          break;
        } else if (str[i] == '"') {
          if (instr == 0)
            instr = 1;
          else
            instr = 0;
        } else if (str[i] != ' ' || instr == 1) {
          argv[word][alen] = str[i];
          alen += 1;
        }
      }

      if (instr == 1) {
        fprintf(stderr, "warning: unfinished string\n");
      }

      argv[word][alen] = '\0';
      argv[word+1] = NULL;
      int nofree[MAX_WORDS];
      for (i = 0; i <= word; i++) {
        if (argv[i][0] == '$') {
          char envval[MAX_WORD_LENGTH];
          int si;
          for (si = 1; argv[i][si] != '\0'; si++) {
            envval[si - 1] = argv[i][si];
          }
          envval[si - 1] = '\0';
          free(argv[i]);
          nofree[i] = 1;
          argv[i] = getenv(envval);
        }
      }

      exec_vect(argv);
      for (i = 0; i <= word; i++) {
        if (nofree[i] != 1)
          free(argv[i]);
      }
    }
    add_history(str);
    free(str);
  }
  return 0;
}
