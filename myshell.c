#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "stdio.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include <string.h>
#include <ctype.h>
#include <limits.h> // Constant from limits.h PATH_MAX (1024 on Linux)

/*
    START of HELPER functions
*/

// Initial set up
// Reads a command from stdin, returns the first line separated by ";"
char *initial(char *command, char **cmdpt) {
    printf("hello: ");
    fflush(stdout);
    if (fgets(command, 1024, stdin) == NULL) return NULL;  // handle EOF
    size_t len = strlen(command);
    if (len > 0 && command[len - 1] == '\n')
        command[len - 1] = '\0';  // remove trailing newline
    return strtok_r(command, ";", cmdpt); // Strtok_r is required for the later usage when the loop iterates per line
}

// Trims leading and trailing spaces/tabs in place
char* trim(char *str) {
    if (!str) return NULL;

    // Trim leading spaces
    while (*str && isspace((unsigned char)*str)) str++;

    // Trim trailing spaces
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    return str;
}

// Splits a line into argv array, returns number of arguments
int parse_command(char *line, char *argv[], int max_args) {
    int i = 0;
    char *linept;
    char *token = strtok_r(line, " ", &linept); // Could use strtok instead since the line is parsed once
    while (token != NULL && i < max_args - 1) { 
        argv[i++] = token;
        token = strtok_r(NULL, " ", &linept);
    }
    argv[i] = NULL;
    return i;
}

/* Does command line end with & */
int amper_check(char *argv[], int argc) {
    if (argc > 0 && strcmp(argv[argc - 1], "&") == 0) {
        argv[argc - 1] = NULL;  // remove '&' from argv
        return 1;               
    }
    return 0;                 
}


/*
    START of ASSIGNMENT functions
*/

/* Redirects:
   " > " redirects output to named file
   " 2> " redirects error output to named file
   " 2>&1 " redirects both error and regular output to named file
*/
void redirect(char *argv[]) {
    int fd;
    for (int j = 0; argv[j] != NULL; j++) {
        if (!strcmp(argv[j], ">") && argv[j+1] != NULL) {
            fd = creat(argv[j+1], 0660);
            if (fd < 0) { perror("creat"); return; }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            argv[j] = argv[j+1] = NULL;
            j++;
        } 
        else if (!strcmp(argv[j], "2>") && argv[j+1] != NULL) {
            fd = creat(argv[j+1], 0660);
            if (fd < 0) { perror("creat"); return; }
            dup2(fd, STDERR_FILENO);
            close(fd);
            argv[j] = argv[j+1] = NULL;
            j++;
        } 
        else if (!strcmp(argv[j], "2>&1")) {  // Use case ex. hello: ls –l myshell nofile > out 2>&1
            dup2(STDOUT_FILENO, STDERR_FILENO); // Expected "> out" before "2>&1"
            argv[j] = NULL;  
        }
    }
}


int handle_cd(int argc, char *argv[]) {
    // If no argument: go to HOME
    char *dir = (argc > 1) ? argv[1] : getenv("HOME");

    // Needed? Verifies successful "HOME" extraction
    if (!dir) {
        fprintf(stderr, "cd: No directory specified, and couldn't extract home path\n");
        return -1; 
    }

    if (chdir(dir) != 0) {
        perror("cd");
        return -1;
    }

    return 0;
}


void handle_mypwd() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("mypwd");
    }
}


int main()
{
    char command[1024];
    char *line; // Line is defined by a command line that ends with ";"
    char *cmdpt;
    int  amper, retid, status, argc;
    char *argv[10];

    while (1)
    {
        line = initial(command, &cmdpt);
        while(line != NULL){

            line = trim(line); // Trims leading and trailing spaces/tabs in place

            argc = parse_command(line, argv, 10); // Splits a line into argv array, returns arg count

            /* Is command not empty */
            if (argv[0] != NULL){

                amper = amper_check(argv, argc);  /* Does command line end with & */
                
                // Handles "cd" command case
                if (strcmp(argv[0], "cd") == 0) {
                    handle_cd(argc, argv);
                    goto next_command; // skip fork/execvp
                }

                // Handles "mypwd" command case
                if (strcmp(argv[0], "mypwd") == 0) {
                    handle_mypwd();
                    goto next_command; // skip fork/execvp
                }

                /* for commands not part of the shell command language */ 

                pid_t pid = fork();

                if (pid < 0) {
                    perror("fork failed");
                    goto next_command; // skip this command and continue shell loop
                }
                else if (pid == 0) 
                { 
                    redirect(argv); // Redirects output

                    execvp(argv[0], argv);
                    perror("execvp");
                    exit(1);
                }
                else{
                    /* parent continues here */
                    if (amper == 0)
                        retid = wait(&status);
                }

            }
            

            next_command:
            line = strtok_r(NULL, ";", &cmdpt);
        }
        
    }
}
