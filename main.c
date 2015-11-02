/*******************************************************************************

  @file        main.c

  @author      Ethan Turkeltaub

  @date        20 October 2015

  @brief       eshell

*******************************************************************************/

#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

extern char **environ;

/*
  Declare our built-in commands (cd, help, exit).
*/
int eshell_cd(char **args);
int eshell_help(char **args);
int eshell_debug(char **args);
int eshell_exit(char **args);

/*
  String versions of the built-in commands.
*/
char *builtin_str[] = {
  "cd",
  "help",
  "debug",
  "exit"
};

int (*builtin_func[]) (char **) = {
  &eshell_cd,
  &eshell_help,
  &eshell_debug,
  &eshell_exit
};

int eshell_num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
}


/*
  Command implementations
*/

/**
  @brief Change directory.
  @param args List of args.  args[0] is "cd".  args[1] is the directory.
  @return Always returns 1, to continue executing.
*/
int eshell_cd(char **args) {

  // There are no arguments, so give an error
  if (args[1] == NULL) {
    fprintf(stderr, "eshell: expected argument for \"cd\"\n");
  }

  // There were arguments, so if we can change directories, do so
  else {
    if (chdir(args[1]) != 0) {
      perror("eshell");
    }
  }

  return 1;
}

/**
  @brief Builtin command: print help.
  @param args List of args.  Not examined.
  @return Always returns 1, to continue executing.
*/
int eshell_help(char **args) {
  int i;
  printf("eshell\n");
  printf("\n");
  printf("Type program names and arguments, and hit enter.\n");
  printf("The following are built in:\n");

  for (i = 0; i < eshell_num_builtins(); i++) {
    printf("  %s\n", builtin_str[i]);
  }

  printf("Use the man command for information on other programs.\n");

  return 1;
}

/**
  @brief Show some debug information.
  @return Always returns 1, to continue executing.
*/
int eshell_debug(char **args) {
  for(char **current = environ; *current; current++) {
    puts(*current);
  }
    // return EXIT_SUCCESS;

  return 1;
}

/**
  @brief Builtin command: exit.
  @param args List of args.  Not examined.
  @return Always returns 0, to terminate execution.
*/
int eshell_exit(char **args) {
  return 0;
}

/**
  @brief Launch a program and wait for it to terminate.
  @param args Null terminated list of arguments (including program).
  @return Always returns 1, to continue execution.
*/
int eshell_launch(char **args) {
  pid_t pid;
  int status;

  pid = fork();
  if (pid == 0) {
    // Child process
    if (execvp(args[0], args) == -1) {
      perror("eshell");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    // Error forking
    perror("eshell");
  } else {
    // Parent process
    do {
      waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

/**
  @brief Load the configuration files.
  @return 1
*/
void eshell_config() {
  FILE * fp;
  char * line = NULL;
  size_t len = 0;
  ssize_t read;
  bool configured[] = {false, false}; // {HOME, PATH}

  fp = fopen("profile", "r");
  if (fp == NULL) {
    printf("eshell: profile does not exist\n");
    exit(EXIT_FAILURE);
  }

  while ((read = getline(&line, &len, fp)) != -1) {
    char *e;
    int index;

    e = strchr(line, '=');
    index = (int)(e - line);

    char key[index];
    char value[len - index];

    // Set key equal to the part of the string before the '='
    for (int i = 0; i < index; i++) {
      key[i] = line[i];
    }

    // Set value equal to the part of the string after the '='
    for (int i = index - 1; i < len + 1; i++) {
      value[i - index] = line[i];
    }

    // Make them actual strings
    key[index] = '\0';
    value[strlen(value)] = '\0';

    if (strncmp(key, "HOME", 4) == 0) {
      configured[0] = true;
    }

    if (strncmp(key, "PATH", 4) == 0) {
      configured[1] = true;
    }

    // Set the environment variable
    setenv(key, value, 1);
  }

  if (configured[0] == false) {
    printf("eshell: HOME is not defined\n");
    exit(EXIT_FAILURE);
  }

  if (configured[1] == false) {
    printf("eshell: PATH is not defined\n");
    exit(EXIT_FAILURE);
  }

  fclose(fp);

  if (line) {
    free(line);
  }
}

/**
  @brief Execute shell built-in or launch program.
  @param args Null terminated list of arguments.
  @return 1 if the shell should continue running, 0 if it should terminate
*/
int eshell_execute(char **args) {
  int i;

  if (args[0] == NULL) {
    // An empty command was entered.
    return 1;
  }

  for (i = 0; i < eshell_num_builtins(); i++) {
    if (strcmp(args[0], builtin_str[i]) == 0) {
      return (*builtin_func[i])(args);
    }
  }

  return eshell_launch(args);
}

#define eshell_RL_BUFSIZE 1024

/**
  @brief Read a line of input from stdin.
  @return The line from stdin.
*/
char *eshell_read_line(void) {
  int bufsize = eshell_RL_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer) {
    fprintf(stderr, "eshell: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    // Read a character
    c = getchar();

    // If we hit EOF, replace it with a null character and return.
    if (c == EOF || c == '\n') {
      buffer[position] = '\0';
      return buffer;
    } else {
      buffer[position] = c;
    }

    position++;

    // If we have exceeded the buffer, reallocate.
    if (position >= bufsize) {
      bufsize += eshell_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      if (!buffer) {
        fprintf(stderr, "eshell: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }
}

#define eshell_TOK_BUFSIZE 64
#define eshell_TOK_DELIM " \t\r\n\a"

/**
  @brief Split a line into tokens (very naively).
  @param line The line.
  @return Null-terminated array of tokens.
*/
char **eshell_split_line(char *line) {
  int bufsize = eshell_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token, **tokens_backup;

  if (!tokens) {
    fprintf(stderr, "eshell: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, eshell_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= bufsize) {
      bufsize += eshell_TOK_BUFSIZE;
      tokens_backup = tokens;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
        free(tokens_backup);
        fprintf(stderr, "eshell: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, eshell_TOK_DELIM);
  }

  tokens[position] = NULL;
  return tokens;
}

#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/**
  @brief Loop getting input and executing it.
*/
void eshell_loop(void) {
  char *line;
  char **args;
  int status;

  do {
    char cwd[1024];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      printf(ANSI_COLOR_BLUE "%s " ANSI_COLOR_MAGENTA "> " ANSI_COLOR_RESET, cwd);
      line = eshell_read_line();
      args = eshell_split_line(line);
      status = eshell_execute(args);

      free(line);
      free(args);
    } else {
      perror("getcwd() error");
    }
  } while (status);
}

/**
  @brief Main entry point.
  @param argc Argument count.
  @param argv Argument vector.
  @return status code
*/
int main(int argc, char **argv) {
  // Load config files, if any.
  eshell_config();

  // Run command loop.
  eshell_loop();

  // Perform any shutdown/cleanup.

  return EXIT_SUCCESS;
}
