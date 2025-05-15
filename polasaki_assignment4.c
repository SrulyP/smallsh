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
#define MAX_BACKGROUND_PROCESSES 50

struct command_line {
    char* argv[MAX_ARGS + 1];
    int argc;
    char* inputFile;
    char* outputFile;
    bool isBackground;
};


// --------------------------------------------  Function Declarations and Global Variables -------------------------------------------- //


void command_chooser(struct command_line * currentCommand);
void exit_program(struct command_line * currentCommand);
void change_directory(struct command_line * currentCommand);
void check_status(void);
void redirect_input(char* inputFile);
void redirect_output(char* outputFile);
void redirect_background(struct command_line * currentCommand);
void redirect_foreground(struct command_line * currentCommand);
int  shell_command(struct command_line * currentCommand);
void check_background_processes(void);
void handle_SIGTSTP(int signal);


int foregroundProcessExitCode = 0;
int bgProcesses[MAX_BACKGROUND_PROCESSES];
int bgProcessesCounter = 0;


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
    // If it is not empty and not a comment, check for input/output files and if should run in background 
    while (token) {
        if (!strcmp(token, "<")) {
            currentCommand -> inputFile = strdup(strtok(NULL, " \n"));
        } else if (!strcmp(token, ">")) {
            currentCommand -> outputFile = strdup(strtok(NULL, " \n"));
        } else if (!strcmp(token, "&")) {
            currentCommand -> isBackground = true;
        } else {
            currentCommand -> argv[currentCommand -> argc++] = strdup(token);
        }
        token = strtok(NULL, " \n");
    }
    return currentCommand;
}


// -------------------------------------------- Main -------------------------------------------- //


int main() {
    // handle SIGINT (ctrl - C)
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    sigaction(SIGINT, &SIGINT_action, NULL);


    // handle SIGTSTP (ctrl - Z)
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = &handle_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    struct command_line * currentCommand;
    while (true) {
        currentCommand = parse_input();
        // If command is not empty and not a comment, run it
        if ((currentCommand->argc > 0) && (currentCommand->argv[0][0] != '#')) {
            command_chooser(currentCommand);
            }
        check_background_processes();
    }
    return EXIT_SUCCESS;
}


// -------------------------------------------- Command chooser -------------------------------------------- //


void command_chooser(struct command_line * currentCommand) {
    // find if current command is one of the 3 built-in commands, or if it is shell command
    if (strcmp(currentCommand -> argv[0], "exit") == 0) {
        exit_program(currentCommand);

    } else if (strcmp(currentCommand -> argv[0], "cd") == 0) {
        change_directory(currentCommand);

    } else if (strcmp(currentCommand -> argv[0], "status") == 0) {
        check_status();

    } else {
        shell_command(currentCommand);
    }
}


// -------------------------------------------- Built-in Commands -------------------------------------------- //


void exit_program(struct command_line * currentCommand) {
    // Kill any other processes or jobs that your shell has started before it terminates itself
    // iterate through bg processes and kill them
    exit(EXIT_SUCCESS);
}

void change_directory(struct command_line * currentCommand) {
    // If no location is provided, go to HOME path.
    if (currentCommand -> argc < 2) {
        char * homePath = getenv("HOME");
        chdir(homePath);
    // Else, go to provided path
    } else {
        char * path = currentCommand -> argv[1];
        if (chdir(path) == -1) {
            printf("%s: no such file or directory\n", path);
            fflush(stdout);
        }
    }
}

void check_status(void) {
    // prints out either the exit status or the terminating signal of the last foreground process ran by shell.
    printf("exit value %d\n", foregroundProcessExitCode);
    fflush(stdout);
}


// -------------------------------------------- Redirection of input and output -------------------------------------------- //


// Redirects the standard input file to match the desired input file
void redirect_input(char * inputFile) {
    int fileDescriptor = open(inputFile, O_RDONLY);
    if (fileDescriptor == -1) {
        printf("cannot open %s for input\n", inputFile);
        fflush(stdout);
        exit(EXIT_FAILURE);
    }
    int redirection = dup2(fileDescriptor, 0);
    if (redirection == -1) {
        perror("dup2()");
        exit(EXIT_FAILURE);
    }
    close(fileDescriptor);
}

// Redirects the standard output file to match the desired output file, creating a new one if it doesn't exist
void redirect_output(char * outputFile) {
    int fileDescriptor = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0640);
    if (fileDescriptor == -1) {
        printf("cannot open %s for output\n", outputFile);
        fflush(stdout);
        exit(EXIT_FAILURE);
    }
    int redirection = dup2(fileDescriptor, 1);
    if (redirection == -1) {
        perror("dup2()");
        exit(EXIT_FAILURE);
    }
    close(fileDescriptor);
}

void redirect_background(struct command_line * currentCommand) {
    // if no input/output files are listed in the command and its a background process, redirect stdin/out to /dev/null
    char * inputFile = "/dev/null";
    char * outputFile = "/dev/null";

    // If there was an input file, redirect stdin to be that file
    if (currentCommand -> inputFile) {
        inputFile = currentCommand -> inputFile;
    }
    redirect_input(inputFile);

    // If there was an output file, redirect stdout to be that file
    if (currentCommand -> outputFile) {
        outputFile = currentCommand -> outputFile;
    }
    redirect_output(outputFile);
}

void redirect_foreground(struct command_line * currentCommand) {
    // If there was an input file and its a foreground process, redirect stdin to be that file
    if (currentCommand -> inputFile) {
        char * inputFile = currentCommand -> inputFile;
        redirect_input(inputFile);
    }

    // If there was an output file, redirect stdout to be that file
    if (currentCommand -> outputFile) {
        char * outputFile = currentCommand -> outputFile;
        redirect_output(outputFile);
    }
}


// -------------------------------------------- Shell Commands -------------------------------------------- //


int shell_command(struct command_line * currentCommand) {
    pid_t pid = fork();
    int childStatus;
    switch (pid) {

    // Failure in fork()
    case -1:
        perror("fork()");
        exit(EXIT_FAILURE);
        break;

    // Child 
    case 0:
        if (currentCommand->isBackground) {
            // If command is run in background, ignore SIGINT (ctrl - z)
            struct sigaction ignore_action = {0}; 
            ignore_action.sa_handler = SIG_IGN;
            sigaction(SIGINT, &ignore_action, NULL);
            redirect_background(currentCommand);
        } else {
        // If command is run in foreground, run SIGINT (ctrl - z)
            struct sigaction default_action = {0};
            default_action.sa_handler = SIG_DFL;  
            sigaction(SIGINT, &default_action, NULL); 
            redirect_foreground(currentCommand);
    }

        // Using execv, run the command
        if (execvp(currentCommand -> argv[0], currentCommand -> argv) == -1) {
            printf("%s: no such file or directory\n", currentCommand->argv[0]);
            fflush(stdout);
            exit(EXIT_FAILURE);
        }
        break;

    // Parent
    default:
        if (!currentCommand -> isBackground) {
            pid_t pidStatus = waitpid(pid, &childStatus, 0);
            
            // Obtain the status of how the child ended:
            // If it was a normal termination, what was the termination code?
            if (WIFEXITED(childStatus)) {
                int status = WEXITSTATUS(childStatus);
                foregroundProcessExitCode = status;
            // If it was an abnormal termination, what was the termination code?
            } else if (WIFSIGNALED(childStatus)) {
                foregroundProcessExitCode = WTERMSIG(childStatus);
                printf("terminated by signal %d\n", foregroundProcessExitCode);
                fflush(stdout);
            }
        } else {
            printf("background pid is %d\n", pid);
            fflush(stdout);
            bgProcesses[bgProcessesCounter] = pid;
            bgProcessesCounter++;
            check_background_processes();
        }
        break;
    }

    return EXIT_SUCCESS;
}

void check_background_processes(void) {
    int status;
    for (int i = 0; i < bgProcessesCounter; i++) {
        pid_t bgPid = bgProcesses[i];
        pid_t currentStatus = waitpid(bgPid, &status, WNOHANG);

        // If the current background process is still running, move on to the next
        if (currentStatus == 0) {
            continue;
        } else if (currentStatus == -1) {
            // If there is an error, or if the process was already reaped
            bgProcesses[i] = bgProcesses[--bgProcessesCounter];
            i--; 
        } else {
            // If the current background process has ended, print the PID and the exit value/signal
            if (WIFEXITED(status)) {
                printf("background pid %d is done: exit value %d\n", bgPid, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("background pid %d is done: terminated by signal %d\n", bgPid, WTERMSIG(status));
            }
            fflush(stdout);

            // Remove this PID from the array and check the one that replaced its position
            bgProcesses[i] = bgProcesses[--bgProcessesCounter];
            i--;
        }
    }
}

void handle_SIGTSTP(int signal) {

}