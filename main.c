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

#define MAX_WORDS 64
#define MAX_WORD_LENGTH 128

int exec_and_wait(int input, int output, const char *file, char *const argv[]) {
  if (strcmp(file, "cd") == 0) {
    char* arg;
    if (argv[1] == NULL) {
      arg = getenv("HOME");
    } else {
      arg = argv[1];
    }
    chdir(argv[1]);
    char abuf[4096];
    getcwd(&abuf, 4096);
    setenv("PWD", abuf, 1);
    return 0;
  } else if (strcmp(file, "exit") == 0) {
    exit(0);
  } else {
    pid_t pid = fork();
    switch (pid) {
      case -1:
        break;
  
      case 0: /* Child */
        int c1 = dup2(input, STDIN_FILENO);
        int c2 = dup2(output, STDOUT_FILENO);
        setvbuf(stdout, NULL, _IONBF, 0);
        execvp(file, argv);
  
      default: /* Parent */
        int stat_loc;
        waitpid(pid, &stat_loc, 0);
        return stat_loc;
    }
  }
  return 0;
}

struct command {
  int input;
  int output;
  char* file;
  char* argv[MAX_WORDS + 1];
};

int exec_vect(char *const argv[]) {
  struct command* commands[sizeof(struct command) * MAX_WORDS];
  int i = 0;
  int c = 0;
  int a = 0;
  struct command cmd;
  cmd.input = STDIN_FILENO;
  cmd.output = STDOUT_FILENO;
  commands[c] = &cmd;
  for (i = 0; argv[i] != NULL; i++) {
    if (strcmp(argv[i], "|") == 0) {
      commands[c]->argv[a] = NULL;
      int pipes[2];
      pipe(pipes);
      commands[c]->output = pipes[1];
      struct command cmd;
      c++; // heh
      a = 0;
      commands[c] = &cmd;
      commands[c]->input = pipes[0];
      commands[c]->output = STDOUT_FILENO;
    } else {
      commands[c]->argv[a] = malloc(MAX_WORD_LENGTH);
      int n;
      for (n = 0; argv[i][n] != '\0'; n++) {
        commands[c]->argv[a][n] = argv[i][n];
      }
      commands[c]->argv[a][n] = '\0';
      a++;
    }
  }
  c++;
  commands[c] = NULL;

  // now, execute all that
  for (i = 0; commands[i] != NULL; i++) {
    if (commands[i]->argv[0] != NULL) {
      exec_and_wait(commands[i]->input, commands[i]->output,
          commands[i]->argv[0], commands[i]->argv);
      // clean up
      int n;
      for (n = 0; commands[i]->argv[n] != NULL; n++) {
        free(commands[i]->argv[n]);
      }
    }
  }
}

int main() {
  char hostname[128];
  gethostname(hostname, 128);
  while (1) {
    printf("%s@%s: %s> ", getenv("USER"), hostname, basename(getenv("PWD")));
    char* str = readline(NULL);
    if (str == NULL) {
      exit(0);
    }

    int len = strlen(str);
    
    if (len > 0) {
      char* argv[MAX_WORDS + 1];
      int i = 0;
      int alen = 0;
      int word = 0;
      argv[0] = malloc(MAX_WORD_LENGTH);
      for (i = 0; i < len; i++) {
        if (str[i] == ' ' && alen > 0) {
          argv[word][alen] = '\0';

          /*if (word == 0) {
            argv[1] = malloc(MAX_WORD_LENGTH);
            int n;
            for (n = 0; n <= alen; n++) {
              argv[1][n] = argv[0][n];
            }
            word += 1;
          }*/

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
        } else if (str[i] != ' ') {
          argv[word][alen] = str[i];
          alen += 1;
        }
      }

      argv[word][alen] = '\0';
      argv[word+1] = NULL;
      exec_vect(argv);
      for (i = 0; i <= word; i++) {
        free(argv[i]);
      }
    }
    add_history(str);
    free(str);
  }
  return 0;
}
