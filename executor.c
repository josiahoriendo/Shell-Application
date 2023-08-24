/* 118889344 */
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <stdlib.h>
#include <err.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "command.h"
#include "executor.h"

#define OPEN_FLAGS (O_WRONLY | O_TRUNC | O_CREAT)
#define DEF_MODE 0664

void check_sys_call(int fd, const char *sys_call);

static int execute_aux(struct tree *t, int p_input_fd, int p_output_fd);

/*
static void print_tree(struct tree *t);
*/

int execute(struct tree *t) {
   /*print_tree(t); */

   return execute_aux(t, STDIN_FILENO, STDOUT_FILENO);
}

/* aux function for execute recursion */
static int execute_aux(struct tree *t, int p_input_fd, int p_output_fd) {
   if (t->conjunction == NONE) {
      if (!strcmp(t->argv[0], "cd")) {
         int stat;
         if (!(t->argv[1]) || !strcmp(t->argv[1], "HOME")) {
            stat = chdir(getenv("HOME"));
         } else {
            stat = chdir(t->argv[1]);
         }
         if (stat == -1) {
            perror(t->argv[1]);
         }
      } else if (!strcmp(t->argv[0], "exit")) {
         exit(EXIT_SUCCESS);
      } else {
         pid_t pid = fork();
         check_sys_call(pid, "fork");

         if (pid > 0) {
            int status;
            wait(&status);
            if (WIFEXITED(status)) {
               if (WEXITSTATUS(status)) {
                  return -1;
               }
            }
         } else {
            int result;

            if (t->input) {
               int check, checker, fd;

               fd = open(t->input, O_RDONLY);
               check_sys_call(fd, "open");

               check = dup2(fd, STDIN_FILENO);
               check_sys_call(check, "dup2");

               checker = close(fd);
               check_sys_call(checker, "close");
            } else {
               int check;
               check = dup2(p_input_fd, STDIN_FILENO);
               check_sys_call(check, "dup2");
            }

            if (t->output) {
               int check, checker, fd;
               fd = open(t->output, OPEN_FLAGS, DEF_MODE);
               check_sys_call(fd, "open");

               check = dup2(fd, STDOUT_FILENO);
               check_sys_call(check, "dup2");

               checker = close(fd);
               check_sys_call(checker, "close");
            } else {
               int check;
               check = dup2(p_output_fd, STDOUT_FILENO);
               check_sys_call(check, "dup2");
            }

            result = execvp(t->argv[0], t->argv);

            if (result == -1) {
               fprintf(stderr, "Failed to execute %s\n", t->argv[0]);
               fflush(stdout);
               exit(EX_OSERR);
            } else {
               exit(EXIT_SUCCESS);
            }
         }
      }
   } else if (t->conjunction == AND) {  
      int value = execute_aux(t->left, p_input_fd, p_output_fd);
      if (!value) {
         int stat;
         pid_t result = fork();
         check_sys_call(result, "fork");
         if (result > 0) {
            waitpid(result, &stat, 0);
            return stat;
         } else if (!result) {
            int val = execute_aux(t->right, p_input_fd, p_output_fd);
            if (!val) {
               exit(EXIT_SUCCESS);
            } else {
               exit(EX_OSERR);
            }
         }
      } else {
         return value;
      }
   } else if (t->conjunction == PIPE) { 
      int pipe_result, pipe_fd[2];
      pid_t first;
      int input = p_input_fd;
      int output = p_output_fd;

      if (t->left->output) {
         printf("Ambiguous output redirect.\n");
         return -1;
      }
      if (t->right->input) {
         printf("Ambiguous input redirect.\n");
         return -1;
      }

      pipe_result = pipe(pipe_fd);
      check_sys_call(pipe_result, "pipe");

      first = fork();
      check_sys_call(first, "fork");

      if (!first) {             /* child */
         int value;

         close(pipe_fd[0]);
         dup2(pipe_fd[1], STDOUT_FILENO);

         value = execute_aux(t->left, p_output_fd, pipe_fd[1]); 
         close(pipe_fd[1]);

         if (!value) {
            exit(EXIT_SUCCESS);
         } else {
            exit(EX_OSERR);
         }
      } else {
         int value;
         int second = fork();

         if (!second) {
            close(pipe_fd[1]);
            dup2(pipe_fd[0], STDIN_FILENO);

            value = execute_aux(t->right, pipe_fd[0], p_output_fd);
            close(pipe_fd[0]);

            if (!value) {
               exit(EXIT_SUCCESS);
            } else {
               exit(EX_OSERR);
            }
         } else {
            close(pipe_fd[1]);
            close(pipe_fd[0]);

            waitpid(first, NULL, 0);
            if (t->input) {
               int checker = close(input);
               check_sys_call(checker, "close");
            }
            waitpid(second, NULL, 0);
            if (t->output) {
               int checker = close(output);
               check_sys_call(checker, "close");
            }
         }

      }

   } else if (t->conjunction == SUBSHELL) {
      pid_t first = fork();
      check_sys_call(first, "fork");
      if (first > 0) {
         int status;
         waitpid(first, &status, 0);
         return status;
      } else if (!first) {
         int value, input = p_input_fd;
         int output = p_output_fd;

         if (t->input) {
            int check;

            input = open(t->input, O_RDONLY);
            check_sys_call(input, "open");

            check = dup2(input, STDIN_FILENO);
            check_sys_call(check, "dup2");

         } else {
            int check;
            check = dup2(input, STDIN_FILENO);
            check_sys_call(check, "dup2");
         }

         if (t->output) {
            int check;
            output = open(t->output, OPEN_FLAGS, DEF_MODE);
            check_sys_call(output, "open");

            check = dup2(output, STDOUT_FILENO);
            check_sys_call(check, "dup2");

         } else {
            int check;
            check = dup2(output, STDOUT_FILENO);
            check_sys_call(check, "dup2");
         }
         value = execute_aux(t->left, input, output);
         if (t->input) {
            int checker = close(input);
            check_sys_call(checker, "close");
         }
         if (t->output) {
            int checker = close(output);
            check_sys_call(checker, "close");
         }
         if (!value) {
            exit(EXIT_SUCCESS);
         } else {
            exit(EX_OSERR);
         }
      }
   }
   return 0;
}

/*
static void print_tree(struct tree *t) {
   if (t != NULL) {
      print_tree(t->left);

      if (t->conjunction == NONE) {
         printf("NONE: %s, ", t->argv[0]);
      } else {
         printf("%s, ", conj[t->conjunction]);
      }
      printf("IR: %s, ", t->input);
      printf("OR: %s\n", t->output);

      print_tree(t->right);
   }
}
*/

void check_sys_call(int fd, const char *sys_call) {
   if (fd < 0) {
      perror(sys_call);
      exit(EX_OSERR);
   }
}
