// Function: Code for parser adapted from code provided in exploration:
// Link: https://canvas.oregonstate.edu/courses/1999732/assignments/9997827?module_item_id=25329384
// Date: 12 May 2025


// -------------------------------------------- Preprocessor Directives and Structs -------------------------------------------- //


#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>


#define INPUT_LENGTH 2048
#define MAX_ARGS 512

struct command_line {
    char * argv[MAX_ARGS + 1];
    int  argc;
    char * input_file;
    char * output_file;
    bool is_bg;
};


// --------------------------------------------  Function Declarations and Global Variables -------------------------------------------- //


void exit_program(struct command_line *currentCommand);
void change_directory(struct command_line *currentCommand);
void check_status();
int  shell_command(struct command_line *currentCommand);
void command_chooser(struct command_line *currentCommand);

int foregroundProcessExitCode = 0;


// -------------------------------------------- Parser - Adapted from Provided Code -------------------------------------------- //


struct command_line * parse_input() {
    char input[INPUT_LENGTH];
    struct command_line * currentCommand = (struct command_line * ) calloc(1,
    sizeof(struct command_line));
    // Get input
    printf(": ");
    fflush(stdout);
    fgets(input, INPUT_LENGTH, stdin);
    // Tokenize the input
    char * token = strtok(input, " \n");
    // Check if the command is a comment or if it is empty
    if (token && (token[0] == '#')) {
        return currentCommand;
    } else if (!token) {
        return currentCommand;
    }
    // If it is not empty and not a comment, check for input/output files and if should run in background 
    while (token) {
        if (!strcmp(token, "<")) {
        currentCommand -> input_file = strdup(strtok(NULL, " \n"));
        } else if (!strcmp(token, ">")) {
        currentCommand -> output_file = strdup(strtok(NULL, " \n"));
        } else if (!strcmp(token, "&")) {
        currentCommand -> is_bg = true;
        } else {
        currentCommand -> argv[currentCommand -> argc++] = strdup(token);
        }
        token = strtok(NULL, " \n");
    }
    return currentCommand;
}


// -------------------------------------------- Main -------------------------------------------- //


int main() {
    struct command_line * currentCommand;
    while (true) {
        currentCommand = parse_input();
        command_chooser(currentCommand);
    }
    return EXIT_SUCCESS;
}


// -------------------------------------------- Command chooser -------------------------------------------- //


void command_chooser(struct command_line *currentCommand) {
// find if current command is one of the 3 built-in commands, or if it is shell command
    if (strcmp(currentCommand->argv[0], "exit") == 0) {
        exit_program(currentCommand);

    } else if (strcmp(currentCommand->argv[0], "cd") == 0) {
        change_directory(currentCommand);

    } else if (strcmp(currentCommand->argv[0], "status") == 0) {
        check_status();

    } else {
        shell_command(currentCommand);
    }
}


// -------------------------------------------- Built-in Commands -------------------------------------------- //


void exit_program(struct command_line *currentCommand) {
    // Kill any other processes or jobs that your shell has started before it terminates itself
    exit(EXIT_SUCCESS);
}

void change_directory(struct command_line *currentCommand) {
    // If no location is provided, go to HOME path.
    if (currentCommand->argc < 2) {
        char* homePath = getenv("HOME");
        chdir(homePath);
    // Else, go to provided path
    } else {
        char* path = currentCommand->argv[1];
        if (chdir(path) == -1) { 
            printf("cd: %s: No such file or directory\n", path);
            fflush(stdout);
        } else { 
            chdir(path);
        }
    }
}

void check_status() {
    // prints out either the exit status or the terminating signal of the last foreground process ran by shell.
    printf("exit value %d", foregroundProcessExitCode);
    } 


// -------------------------------------------- Shell Commands -------------------------------------------- //


int shell_command(struct command_line *currentCommand) {
    if (currentCommand->is_bg) {
        // make list to keep track of background functions
    }
    pid_t pid = fork();
    int childStatus;
    switch (pid){
        case -1:
            perror("fork failed!");
            break;
        case 0:
            // Using execv, run the command
            if (execvp(currentCommand->argv[0], currentCommand->argv) == -1) {
                perror("execvp failed!");
                exit(1);
            }
            break;
        default:
            //parent
            break;
    }
    pid_t pidStatus = waitpid(pid, &childStatus, 0);
    // Obtain the status of how the child ended
    // If it was a normal termination, what was the term. code?
    if (WIFEXITED(childStatus)) {
        int status = WEXITSTATUS(childStatus);
        foregroundProcessExitCode = status;
    // If it was an abnormal termination, what was the term. code?
    } else if (WIFSIGNALED(childStatus)) {
        foregroundProcessExitCode = WTERMSIG(childStatus);
    }
    return 0;
}


