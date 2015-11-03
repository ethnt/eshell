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
  Declare built-in commands
*/
int eshell_cd(char **args);
int eshell_help(char **args);
int eshell_debug(char **args);
int eshell_exit(char **args);

/*
  String versions of the built-in commands
*/
char *builtin_str[] = {
  "cd",
  "help",
  "debug",
  "exit"
};

/*
  Make an array of built-in commands
*/
int (*builtin_func[]) (char **) = {
  &eshell_cd,
  &eshell_help,
  &eshell_debug,
  &eshell_exit
};

/*
  Return the number of built-in commands
*/
int eshell_num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
}

/**
  @brief       Change the working directory.
  @param  args List of arguments, where args[0] is "cd" and args[1] is the
                 directory to change to.
  @return      Always return 1 to continue executing the shell.
*/
int eshell_cd(char **args) {
  if (args[1] == NULL) {
    // There was no directory passed, so error out
    fprintf(stderr, "eshell: expected argument for \"cd\"\n");
  } else {
    // There was a directory passed, so change the directory
    if (chdir(args[1]) != 0) {
      perror("eshell: could not change directory");
    }
  }

  return 1;
}

/**
  @brief       Print a bit of help.
  @param  args Arguments that are ignored.
  @return      Always return 1 to continue executing the shell.
*/
int eshell_help(char **args) {
  int i;
  printf("eshell\n");
  printf("\n");
  printf("There are a few programs built-in:\n");

  // Print out all of the built-in commands
  for (i = 0; i < eshell_num_builtins(); i++) {
    printf("  %s\n", builtin_str[i]);
  }

  return 1;
}

/**
  @brief       Show some debug information.
  @param  args Arguments that are ignored.
  @return      Always return 1 to continue executing the shell.
*/
int eshell_debug(char **args) {
  // Print out all of the environment variables that are currently defined.
  for(char **current = environ; *current; current++) {
    puts(*current);
  }

  return 1;
}

/**
  @brief       Exit the shell.
  @param  args Arguments that are ignored.
  @return      Always returns 0.
*/
int eshell_exit(char **args) {
  return 0;
}

/**
  @brief       Launches an external program.
  @param  args List of arguments, including the program to execute.
  @return      Always return 1 to continue executing the shell.
*/
int eshell_launch(char **args) {
  pid_t pid;
  int status;

  // Make a copy of the currently running process
  pid = fork();

  // If the PID is 0, make that process a child process
  if (pid == 0) {
    // Make the child process run the program desired
    if (execvp(args[0], args) == -1) {
      perror("eshell: child process failed\n");
    }

    exit(EXIT_FAILURE);

  // The PID is less than 0, so it's a forking error
  } else if (pid < 0) {
    perror("eshell: error forking parent process\n");

  // Fork executed properly
  } else {
    // Wait for the process to finish — try once, then if it's exited, stop the
    //   loop
    do {
      waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

/**
  @brief       Execute shell built-in or launch program.
  @param  args Null terminated list of arguments.
  @return      0 if the shell should terminate, non-zero if the shell continues
                 to run.
*/
int eshell_execute(char **args) {
  int i;

  // An empty command was entered, just show the loop again
  if (args[0] == NULL) {
    return 1;
  }

  // It was a legitamite program, so loop through all of the built-in commands
  for (i = 0; i < eshell_num_builtins(); i++) {

    // Found a command that matches the one passed
    if (strcmp(args[0], builtin_str[i]) == 0) {

      // Run the built-in program
      return (*builtin_func[i])(args);
    }
  }

  // A command was passed but it wasn't a built-in one, so try and launch it
  //   externally
  return eshell_launch(args);
}

#define ESHELL_RL_BUFSIZE 1024

/**
  @brief  Read a line of input from stdin.
  @return The line from stdin.
*/
char *eshell_read_line(void) {
  int bufsize = ESHELL_RL_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  // Isn't enough buffer space, exit out.
  if (!buffer) {
    fprintf(stderr, "eshell: allocation error\n");

    exit(EXIT_FAILURE);
  }

  // Read the whole input buffer
  while (1) {

    // Read the next character
    c = getchar();

    if (c == EOF || c == '\n') {
      // If we hit EOF, replace it with a null character and return
      buffer[position] = '\0';

      return buffer;
    } else {
      // Otherwise, put the character in the right position in the buffer
      buffer[position] = c;
    }

    position++;

    // We've exceeded the buffer size
    if (position >= bufsize) {

      // Increase the buffer size
      bufsize += ESHELL_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);

      if (!buffer) {
        fprintf(stderr, "eshell: allocation error\n");

        exit(EXIT_FAILURE);
      }
    }
  }
}

#define ESHELL_TOK_BUFSIZE 64
#define ESHELL_TOK_DELIM " \t\r\n\a"

/**
  @brief       Super naively split a line into tokens
  @param  line The line to tokenize
  @return      Null-terminated array of tokens
*/
char **eshell_split_line(char *line) {
  int bufsize = ESHELL_TOK_BUFSIZE
  int position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token;
  char **tokens_backup;

  // There aren't any tokens, so something went wrong with allocation
  if (!tokens) {
    fprintf(stderr, "eshell: allocation error\n");

    exit(EXIT_FAILURE);
  }

  // Start the tokenization
  token = strtok(line, ESHELL_TOK_DELIM);
  while (token != NULL) {
    // Add each token to the array
    tokens[position] = token;

    position++;

    // We've reached the end of the buffer
    if (position >= bufsize) {

      // Attempt to reallocate some memory to increase the buffer size
      bufsize += ESHELL_TOK_BUFSIZE;
      tokens_b = tokens;
      tokens = realloc(tokens, bufsize * sizeof(char*));

      // If there's still no tokens, quit
      if (!tokens) {
        free(tokens_b);
        fprintf(stderr, "eshell: allocation error\n");

        exit(EXIT_FAILURE);
      }
    }

    // Run the tokenization again to get the next toekn
    token = strtok(NULL, ESHELL_TOK_DELIM);
  }

  tokens[position] = NULL;

  return tokens;
}

#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/**
  @brief  Load the configuration files.
  @return Return 0 if configuration loads successfully, otherwise exit with
            non-zero.
*/
void eshell_config() {
  FILE * fp;
  char * line = NULL;
  size_t len = 0;
  ssize_t read;
  bool configured[] = {false, false}; // {HOME is defined, PATH is defined}

  // Open the file `profile`
  fp = fopen("profile", "r");

  // The `profile` file doesn't exist, don't start
  if (fp == NULL) {
    printf("eshell: profile does not exist\n");

    exit(EXIT_FAILURE);
  }

  // Read each line of the file
  while ((read = getline(&line, &len, fp)) != -1) {
    const char s[2] = "=";
    char *token;
    char *vrbl[2];
    int counter = 0;

    // Super naively tokenize each line
    token = strtok(line, s);

    while (token != NULL) {
      // Set token to element 0 if it's the key, 1 if it's the value
      vrbl[counter] = token;

      // Re-run strtok to get the next token
      token = strtok(NULL, s);

      // Incement the counter
      counter++;
    }

    // If HOME was defined, turn its element in the configured array to true
    if (strncmp(vrbl[0], "HOME", 4) == 0) {
      configured[0] = true;
    }

    // If PATH was defined, turn its element in the configured array to true
    if (strncmp(vrbl[0], "PATH", 4) == 0) {
      configured[1] = true;
    }

    // Finally set the variable
    setenv(vrbl[0], vrbl[1], 1);
  }

  // If one of the elements in the configured array isn't true, error out
  if (configured[0] == false) {
    printf("eshell: HOME is not defined\n");

    exit(EXIT_FAILURE);
  } else if (configured[1] == false) {
    printf("eshell: PATH is not defined\n");

    exit(EXIT_FAILURE);
  }

  // Close the file
  fclose(fp);

  if (line) {
    free(line);
  }
}


/**
  @brief Loop getting input and executing it.
*/
void eshell_loop(void) {
  char *line;
  char **args;
  int status;

  // Infinitely loop while the return value for executing commands is non-zeo
  do {
    char cwd[1024];

    // Make sure we have a working directory
    if (getcwd(cwd, sizeof(cwd)) != NULL) {

      // Print a pretty prompt
      printf(ANSI_COLOR_BLUE "%s " ANSI_COLOR_MAGENTA "> " ANSI_COLOR_RESET, cwd);

      // Read the line
      line = eshell_read_line();

      // Split the line
      args = eshell_split_line(line);

      // Execute the command passed and get back a status
      status = eshell_execute(args);

      free(line);
      free(args);
    } else {
      // Something went wrong with the current directory
      perror("getcwd() error");
    }
  } while (status);
}

/**
  @brief      Main entry point.
  @param argc Argument count.
  @param argv Argument vector.
  @return status code
*/
int main(int argc, char **argv) {
  // Load the configuration
  eshell_config();

  // Run the main loop
  eshell_loop();

  return EXIT_SUCCESS;
}
