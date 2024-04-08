#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <glob.h>

#define MAX_LINE 1024
#define MAX_ARGS 128
#define TOKEN_DELIM " \t\r\n\a"

int last_exit_status = 0;

// Function prototypes
void loop();
char *read_line();
char **split_line(char *);
int execute(char **args);
int launch(char **args);
int execute_builtin(char **args);
int cd(char **args);
int pwd(char **args);
int mysh_which(char **args);
int mysh_exit(char **args);

// List of builtin commands, followed by their corresponding functions.
char *builtin_str[] = {
    "cd",
    "pwd",
    "which",
    "exit"
};

int (*builtin_func[]) (char **) = {
    &cd,
    &pwd,
    &mysh_which,
    &mysh_exit
};

int num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
}


/*void loop(void) {
    char *line;
    char **args;
    int status;

    do {
        printf("> ");
        line = read_line();
        args = split_line(line);
        status = execute(args);
        
        free(line);
        free(args);

    } while (status);
}*/

void loop(void) {
    char *line;
    char **args;
    int status = 1;

    while (status) {
        printf("> ");
        line = read_line();
        
        // Check if read_line returned NULL, indicating EOF or read error
        if (line == NULL) {
            if (feof(stdin)) {
                // If we've reached EOF, gracefully exit the loop
                status = 0;
            } else {
                // If there was an error reading the line, you can handle it differently
                // For example, print an error message or attempt to recover
                perror("read_line error");
                status = 0; // Set to 0 to break the loop in case of error
            }
            break; // Break the loop either way
        }

        args = split_line(line);
        if (args[0] == NULL) {
            // If split_line returned an empty command, just free memory and prompt again
            free(line);
            free(args);
            continue; // Continue to the next iteration of the loop
        }

        status = execute(args);

        free(line);
        free(args);
    }

    // Optionally print a goodbye message or perform any cleanup before exiting
    printf("Exiting shell.\n");
}


char *read_line(void) {
    char *line = NULL;
    ssize_t bufsize = 0; // have getline allocate a buffer for us
    getline(&line, &bufsize, stdin);
    return line;
}

char **split_line(char *line) {
    int bufsize = MAX_ARGS, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "mysh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, TOKEN_DELIM);
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += MAX_ARGS;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "mysh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, TOKEN_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

int execute(char **args) {
    if (args[0] == NULL) {
        // An empty command was entered.
        return 1;
    }

    for (int i = 0; i < num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    return launch(args);
}


int cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "mysh: expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("mysh");
        }
    }
    return 1;
}

int pwd(char **args) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("mysh");
    }
    return 1;
}

int mysh_which(char **args) {
    if (args[1] == NULL || args[2] != NULL) {
        fprintf(stderr, "mysh: expected one argument to \"which\"\n");
        return 1;
    }

    char *paths[] = {"/usr/local/bin", "/usr/bin", "/bin"};
    char path[1024];
    for (int i = 0; i < 3; i++) {
        snprintf(path, sizeof(path), "%s/%s", paths[i], args[1]);
        if (access(path, X_OK) == 0) {
            printf("%s\n", path);
            return 1;
        }
    }

    if (strcmp(args[1], "cd") == 0 || strcmp(args[1], "pwd") == 0 || 
        strcmp(args[1], "which") == 0 || strcmp(args[1], "exit") == 0) {
        printf("mysh: %s: shell built-in command\n", args[1]);
    } else {
        fprintf(stderr, "mysh: %s: Command not found\n", args[1]);
    }
    return 1;
}

int mysh_exit(char **args) {
    int i = 1;
    while (args[i] != NULL) {
        printf("%s ", args[i]);
        i++;
    }
    printf("\n");
    exit(0);
}

int main(int argc, char **argv) {
    // Main entry point of the shell

    // If batch mode
    if (argc == 2) {
        // Redirect standard input to read from the file
        FILE *file = fopen(argv[1], "r");
        if (!file) {
            fprintf(stderr, "mysh: Cannot open file %s\n", argv[1]);
            exit(EXIT_FAILURE);
        }
        dup2(fileno(file), STDIN_FILENO);
        fclose(file);
    }

    printf(isatty(STDIN_FILENO) ? "Welcome to my shell!\n" : "");

    // Run command loop.
    loop();

    printf(isatty(STDIN_FILENO) ? "Exiting my shell.\n" : "");

    // Perform any shutdown/cleanup.

    return EXIT_SUCCESS;
}

// Function to expand wildcards; integrate this into your command processing.
void expand_wildcards(char ***args) {
    glob_t glob_result;
    memset(&glob_result, 0, sizeof(glob_result));

    for (int i = 0; (*args)[i] != NULL; i++) {
        if (strchr((*args)[i], '*') != NULL) { // Check if argument contains a wildcard
            glob((*args)[i], GLOB_TILDE, NULL, &glob_result);
            // Replace the wildcard argument with the list of matching files
            for (unsigned j = 0; j < glob_result.gl_pathc; j++) {
                // Note: reallocate (*args) and insert all matches
                printf("Wildcard match: %s\n", glob_result.gl_pathv[j]); // For demonstration
            }
            globfree(&glob_result);
        }
    }
}
#include <fcntl.h> // For file control options

// This function sets up redirection in the shell.
// It modifies file descriptors for the current process based on the command.
// It should be called before executing the command.
int setup_redirection(char **args) {
    int inRedirect = -1, outRedirect = -1; // File descriptors for input and output redirection
    char *inputFile = NULL, *outputFile = NULL;

    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) { // Input redirection
            inputFile = args[i + 1];
            if (inputFile == NULL) {
                fprintf(stderr, "mysh: expected file name after '<'\n");
                return -1;
            }
            inRedirect = open(inputFile, O_RDONLY);
            if (inRedirect < 0) {
                perror("mysh: open input");
                return -1;
            }
            args[i] = NULL; // Remove the redirection from arguments
        } else if (strcmp(args[i], ">") == 0) { // Output redirection
            outputFile = args[i + 1];
            if (outputFile == NULL) {
                fprintf(stderr, "mysh: expected file name after '>'\n");
                return -1;
            }
            // Open file for writing, create if necessary, truncate to zero length.
            outRedirect = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if (outRedirect < 0) {
                perror("mysh: open output");
                return -1;
            }
            args[i] = NULL; // Remove the redirection from arguments
        }
    }

    // Apply the redirections to the process
    if (inRedirect != -1) {
        if (dup2(inRedirect, STDIN_FILENO) < 0) {
            perror("mysh: dup2 input");
            return -1;
        }
        close(inRedirect);
    }
    if (outRedirect != -1) {
        if (dup2(outRedirect, STDOUT_FILENO) < 0) {
            perror("mysh: dup2 output");
            return -1;
        }
        close(outRedirect);
    }

    return 0; // Indicate success
}
int launch(char **args) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        // Child process
        if (setup_redirection(args) == -1) {
            // Handle errors in redirection setup
            exit(EXIT_FAILURE);  // Use EXIT_FAILURE for unsuccessful execution
        }
        if (execvp(args[0], args) == -1) {
            perror("mysh");
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        // Error forking
        perror("mysh");
    } else {
        // Parent process
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

