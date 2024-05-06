//header files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_LINE_LENGTH 512 //maximum length of an input line
#define MAX_NUM_TOKENS 32 //maximum number of tokens in a command

//this function checks the return value of system calls, if return value is -1, the perror message is printed and the program exits with failure status
void check_sys_call(int ret, char* msg) { 
    if (ret == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}


//this function parses the input line into tokens, it takes the input line and creates an array called token to store the parsed tokens
void parse_input(char *line, char *tokens[]) {
    char *token;
    int token_count = 0;

    token = strtok(line, " \n"); // this function will tokenize the input line (split it) based on whitespaces and newline characters
    while (token != NULL && token_count < MAX_NUM_TOKENS - 1) {
        if (strcmp(token, "<") == 0 || strcmp(token, ">") == 0 || strcmp(token, "|") == 0 || strcmp(token, "&") == 0) {
            tokens[token_count++] = strdup(token); // it checks each token for special characters and copies them into the token array
        } else {
            tokens[token_count++] = token;
        }
        token = strtok(NULL, " \n"); 
    }
    tokens[token_count] = NULL; //the tokens array is ended with a null pointer
}


//this function is responsible for executing the parsed command;  handles input/output redirection, piping, and running commands in the background
void execute_command(char *tokens[]) {
    if (tokens[0] == NULL) {
        fprintf(stderr, "ERROR: No command provided\n"); //checks if firsts token is NULL; if so no command was provided so error is printed and returns
        return;
    }


    // declaration of variables: flags are to indicate if a pipe or background exec is requested
    int pipe_flag = 0;
    int background_flag = 0;

    int num_commands = 1; // counts number of commands/pipelines to be exec
    int command_index[MAX_NUM_TOKENS]; // stores the number of tokens in array
    int pipe_fd[2]; //file descripters for the pipe.

    for (int i = 0; tokens[i] != NULL; i++) { //this loop iterates through the pipes to indetify pipe and background requests and stores the index of next command to array.
        if (strcmp(tokens[i], "|") == 0) {
            pipe_flag = 1;
            num_commands++;
            command_index[num_commands - 1] = i + 1;
        } else if (strcmp(tokens[i], "&") == 0) {
            background_flag = 1;
        }
    }

    for (int i = 0; i < num_commands; i++) { //executes commands in the pipeline
        pid_t pid; // forks child process for each command
        int status;

        if (pipe_flag && i < num_commands - 1) {
            check_sys_call(pipe(pipe_fd), "pipe failed"); //handles piping
        }

        pid = fork(); //a new process is created by duplicating
        check_sys_call(pid, "fork failed"); // checks if system is in failure or not and reports if so.

        //in this part, we execute the parent and child process. If pid is 0, it is the child process, else it is the parent
        if (pid == 0) { //child
            if (pipe_flag) { //if the there is a pipe needed, this part will check for the pipe flag. 
                if (i == 0 && num_commands > 1) { // is its the first command in the pipeline and there are more than one command, the standard output is directed to the writend end of the pipe and the reading end is closed, this is of course checked for errors.
                    check_sys_call(dup2(pipe_fd[1], STDOUT_FILENO), "dup2 failed"); 
                    check_sys_call(close(pipe_fd[0]), "close failed");
                } else if (i == num_commands - 1) { //if this is the last command in the pipeline, the standard input is directed to the reading end of the pipe and the writing end is closed since the command doesnt write to it.
                    check_sys_call(dup2(pipe_fd[0], STDIN_FILENO), "dup2 failed");
                    check_sys_call(close(pipe_fd[1]), "close failed");
                } else { // any commands in the middle of the pipeline have their standard inputs and outputs directed to the corresponding ends; reading end to stdin and writng to stdout. AGAIN ALL ARE CHECKED.
                    check_sys_call(dup2(pipe_fd[0], STDIN_FILENO), "dup2 failed");
                    check_sys_call(dup2(pipe_fd[1], STDOUT_FILENO), "dup2 failed");
                }
            }
            //this part handles the redirection using the <,> symbols
            if (strcmp(tokens[i], "<") == 0) { //this checks if the tokens have <, they are for reading
                int fd = open(tokens[i + 1], O_RDONLY); // it opens the file for reading only
                check_sys_call(fd, "open failed");//checks if the opening worked
                check_sys_call(dup2(fd, STDIN_FILENO), "dup2 failed");//duplicates the file descriptor to the input file descriptor and redirects the standard input to the specificed file
                close(fd);
                i++;
            } else if (strcmp(tokens[i], ">") == 0) { // if the token has the < symbol; they are for writing
                int fd = open(tokens[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666); // it opens the file for writing only, creates it if it doesnt exist, or makes it zero lenght if it does and sets permissions to 0666 for unix file creation or modification
                check_sys_call(fd, "open failed");//it chekcs if the file opened right
                check_sys_call(dup2(fd, STDOUT_FILENO), "dup2 failed"); // duplicate the file descriptor onto the standard output file descriptor
                close(fd);
                i++;
            }
            execvp(tokens[command_index[i]], &tokens[command_index[i]]); //this step executes the command regardless of whether the input/output redirection was performed
            perror("execvp failed"); // if the above step fails, this reads out the error message.
            exit(EXIT_FAILURE);
        } else { //after forkingthe child process, this handles the behavior of the parent process
            if (background_flag && i == num_commands - 1) { //if the command is the last command and has a background flag, the loop is broken to ensure the parent process doesnt wait for the background command to finish if its the last command.
                break;
            }

            if (pipe_flag && i < num_commands - 1) { //if the pipe flag is set and its NOT the last command
                check_sys_call(close(pipe_fd[0]), "close failed"); //the parent process closes the pipe file descriptors to release the resources
                check_sys_call(close(pipe_fd[1]), "close failed");
            }
            waitpid(pid, &status, 0); // here we wait for the child process to change state, and the parent will wait indefinitly for the child process to terminate
            if (WIFEXITED(status)) { // here we can make sure that the child process is terminated normally and ideally there would be some indication, but I hadn't worked out what that would be.
            }
        }
    }

    for (int i = 0; tokens[i] != NULL; i++) { //we repeat this process for each element in the token array until we reach the end NULL
        free(tokens[i]); //and after each iteration, I free the up the memory alloted to each token. this is just for memory management
    }
}


//main function where the shell program starts execution.
int main(int argc, char *argv[]) {
    int interactive_mode = 1; // this enables the interactive mode to indicate that the shell is active and running
    if (argc > 1 && strcmp(argv[1], "-n") == 0) { // but if the command line argument is -n, which means the shell is to be operated in non-inteteractive mode, the interactive mode is therefore off.
        interactive_mode = 0;
    }

    char line[MAX_LINE_LENGTH]; //since we already defined the limits for these variables, here we just declare them
    char *tokens[MAX_NUM_TOKENS];

    while (1) { // we use while(1) to indicate that this loop should run continuously until something inside actively terminates it.
        if (interactive_mode) {
            printf("my_shell$ "); // while interactive mode is turned on, the prompt is printed as "my_shell$" indicating an area to type an input
        }

        if (fgets(line, sizeof(line), stdin) == NULL) {  // here we collect the line of input read from standard input stream (STDIN) and store it into the character input of line. But we also only read as many characters as specificed by MAX_LINE_LENGTH. BUT if fgets returns a NULL, there could be an error or...
            if (feof(stdin)) { //the end of input is signalled (i think this could be a ctrl+d on unix)
                printf("\n");//the newline character is printed to put the prompt on the next line. 
            }
            break;// and breaks out of this MAIN loop
        }

        parse_input(line, tokens); // this calls the parse_input function to tokenize the input line retrieved from the line input and store them in the tokens array; as discussed earlier, this function should be able to recognize the special characters as well.

        execute_command(tokens);// this calls the execute_command function to interpret the tokens, handle input/output redirection, piping, and background execution, and then executes the commands.
    }

    return 0;
}
