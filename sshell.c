#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>

#define CMDLINE_MAX 512
#define ARGS_MAX 16
#define TOKEN_MAX 32
#define PATH_MAX 100
#define PIPE_OUTPUT 2


typedef struct argParser
{
    char* args[ARGS_MAX + 1];
    bool redirect;
    bool isOutputAppend;
    char* redirectFile;
    char* cmdPtr;
    pid_t pid;
    bool background;

    struct argParser* nextCmd;

} argParser;

typedef struct job
{
    struct job *next;
    char *command;
    pid_t pid;
} job;

void pipeRedirect(int pipefd, int redirectfd)
{
    dup2(pipefd, redirectfd);
    close(pipefd);
}

void executeParser(argParser* parser)
{
    execvp(parser->args[0], parser->args);
    fprintf(stderr, "Failed to execute command '%s'\n", parser->args[0]);
    exit(1);
}

void redirectAndExecute(
                        int pipeRead, 
                        int redirectRead, 
                        int pipeWrite, 
                        int redirectWrite, 
                        argParser* parser
                        )
{
    pipeRedirect(pipeRead, redirectRead);
    pipeRedirect(pipeWrite, redirectWrite);
    executeParser(parser);
}


void executePipeChain(argParser* head, int numPipeCommands)
{
    argParser* current = head;

    int pipes[numPipeCommands - 1][PIPE_OUTPUT];
    for (int i = 0; i < numPipeCommands - 1; i++)
    {
        pipe(pipes[i]);
    }

    for(int i = 0; i < numPipeCommands; i++)
    {
        current->pid = fork();
        if (current->pid < 0)
        {
            fprintf(stderr, "Child fork failed\n");
            exit(1);
        }
        else if (current->pid == 0)
        {
            /* Child Successfully forked */
            
            if (i == 0)
            {
                /* If first command in pipeline */
                dup2(pipes[0][1], STDOUT_FILENO);
                for (int j = 0; j < numPipeCommands; j++)
                {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
            }
            else if (i == numPipeCommands - 1)
            {
                /* if last command in pipeline */
                dup2(pipes[i-1][0], STDIN_FILENO);
                for (int j = 0; j < 2; j++)
                {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
            }
            else
            {
                dup2(pipes[i-1][0], STDIN_FILENO);         //command 2 reads from command 1
                dup2(pipes[i][1], STDOUT_FILENO);        //command 2 writes to command 3
                for(int i = 0; i < numPipeCommands - 1; i++)
                {
                    close(pipes[0][i]);
                    close(pipes[1][i]);
                }
                // execvp(current->args[0], current->args);
                // fprintf(stderr, "Command2 not found\n");
                // exit(0);
                // pipeRedirect(pipes[i - 1][0], STDIN_FILENO);
                
                // close(pipes[i - 1][1]);
                
                // pipeRedirect(pipes[i][1], STDOUT_FILENO);
                // close(pipes[i][0]);
                
                // for (int j = 0; j < numPipeCommands - 1; j++)
                // {
                //     if (j != i-1 && j != i)
                //     {
                //         close(pipes[j][0]);
                //         close(pipes[j][1]);
                //     }
                // }
                // for(int i = 0; i < 2; i++)
                // {
                //     close(pipes[i - 1][i]);
                //     close(pipes[i][i]);
                // }
            }
            executeParser(current);
        }
        current = current->nextCmd;
    }
    for(int i = 0; i < 2; i++)
    {
        close(pipes[0][i]);
        close(pipes[1][i]);
    }
}

void makePipe(argParser* firstParser, argParser* secondParser)
{
    int fd[2];

    if (pipe(fd) < 0)
    {
        fprintf(stderr, "Pipe could not be initiated\n");
        exit(1);
    }

    firstParser->pid = fork();
    if (firstParser->pid == 0)
    {
        /* Child 1 Execution */

        close(fd[0]);
        pipeRedirect(fd[1], STDOUT_FILENO);
        // dup2(fd[1], STDOUT_FILENO);
        // close(fd[1]);
        // execvp(firstParser->args[0], firstParser->args);
        // fprintf(stderr, "Error: unable to spawn child\n");
        executeParser(firstParser);
    }
    else if (firstParser->pid < 0)
    {
        fprintf(stderr, "Child fork failed\n");
    }

    secondParser->pid = fork();
    if (secondParser->pid == 0)
    {
        close(fd[1]);
        pipeRedirect(fd[0], STDIN_FILENO);
        executeParser(secondParser);
    }
    else if (secondParser->pid < 0)
    {
        fprintf(stderr, "Child fork failed\n");
    }

    close(fd[0]);
    close(fd[1]);
}


void initArgParser(argParser* args)
{
    for (int i = 0; i < ARGS_MAX + 1; i++)
    {
        // args->args[i] = (char*)malloc(TOKEN_MAX + 1); 
        args->args[i] = NULL;
    //     if (!args->args[i])
    //     {
    //         perror("Error: Memory not available");
    //         exit(1);
    //     }
    }
    args->redirect = false;
    args->isOutputAppend = false;
    args->redirectFile = (char*)malloc(TOKEN_MAX + 1);
}

void freeArgParser(argParser* args)
{
    for (int i = 0; i < ARGS_MAX + 1; i++)
    {
        if(args->args[i] != NULL)
            free(args->args[i]);
    }
    free(args->redirectFile);
        
}

void print_list(struct argParser * head)
{
    struct argParser * temp_cmd = head;

    while( temp_cmd != NULL) 
    {
        printf("%s\n", temp_cmd->cmdPtr);
        temp_cmd = temp_cmd->nextCmd;
    }
}

// Referenced from https://www.learn-c.org/en/Linked_lists
void push(struct argParser** head, char* argsString)
{
    /* Push to end of the list */
    struct argParser* new_node = malloc(sizeof(struct argParser));
    new_node->cmdPtr = (char*)malloc(sizeof(char)*CMDLINE_MAX);
    strcpy(new_node->cmdPtr,argsString);
    new_node->nextCmd = NULL;
    initArgParser(new_node);
    
    if(*head == NULL)
    {
        printf("Making new node\n");
        *head = new_node;
    }
    else
    {
        printf("Adding node\n");
        struct argParser* end_node = *head;
        while(end_node->nextCmd != NULL)
        {
            end_node = end_node->nextCmd;
        }
        end_node->nextCmd = new_node;
    }
}

int scanArgs(struct argParser** args, char* argsString)
{
    char* argcopy = (char*)malloc(CMDLINE_MAX + 1);
    strcpy (argcopy, argsString);
    char* token = strtok(argcopy, "|");

    int pipeCount = -1; // Offset of one to account for pipe instead of tokens
    while(token != NULL)
    {
        pipeCount++;
        push(args, token);
        token = strtok(NULL, "|");
    }
    // print_list(args);

    free(argcopy);
    return pipeCount;
}

int checkRedirect(argParser* args, char* cmd)
{
    char* redirectChar = strchr(cmd, '>');      // redirectChar = string after '>' from token
    if (redirectChar != NULL)               // if there is an existing token after '>' 
    {
        char* cmdCopy = (char*)malloc(sizeof(char) * CMDLINE_MAX);
        strcpy(cmdCopy, cmd);
        
        // Get the word after the output redirection '>' 
        char* token = strtok(cmd, ">");
        token = strtok(NULL, ">");

        while (*token != ' ') // To get rid of leading white space if any
            token++;
        
        args->redirect = true;
        
        char* fileName;
        fileName = strtok(token, " ");
        strncpy(args->redirectFile, fileName, TOKEN_MAX);

        token = strtok(NULL, " ");
        if (token != NULL)
        {
            fprintf(stderr, "Error: mislocated output redirection\n");
            return -1;
        }
        free(cmdCopy);
    }

    return 0;
}

int checkOutputAppending(argParser* args, char* argString)
{
    char* outputAppendPosition = strstr(argString, ">>");
    
    if (outputAppendPosition == NULL)
        /* Didn't find output appending */
        return 0;
    
    *(outputAppendPosition++) = '\0';
    *(outputAppendPosition++) = '\0';
    
    // while (*outputAppendPosition == '\0')
    // {
    //     if (*outputAppendPosition != ' ')
    //     {
    //         fprintf(stderr, "Error: mislocated output redirection\n");
    //         return -1;
    //     }
    //     outputAppendPosition++;
    // }
    while (*outputAppendPosition == ' ')
        outputAppendPosition++;

    /* Check to see if there are any more tokens after the file name*/
    char* token = strtok(outputAppendPosition, " ");
    token = strtok(NULL, " ");
    if (token != NULL)
    {
        fprintf(stderr, "Error: mislocated output redirection\n");
        return -1;
    }

    args->redirect = true;
    args->isOutputAppend = true;
    strcpy(args->redirectFile, outputAppendPosition);

    return 0;
}

int checkBackground(argParser* args, char* argsString)
{
    char* backgroundChar = strchr(argsString, '&');
    if(backgroundChar != NULL)
    {   
        *backgroundChar = ' '; // Get rid of the & in argument string
        backgroundChar++;
        while(*backgroundChar != '\0')
        {
            if (*(backgroundChar) != ' ')
            {
                fprintf(stderr, "Error: mislocated background sign\n");
                return -1;
            }
            backgroundChar++;
        }
        args->background = true;
    }

    return 0;
}

int makeArgs(argParser* args, char* argsString)
{
    // printf("Making Args\n");
    char* argcopy = (char*)malloc(CMDLINE_MAX + 1);
    
    strcpy(argcopy, argsString);

    if (checkOutputAppending(args, argcopy))
    {
        return -1;
    }
    if (checkBackground(args, argcopy))
    {
        return -1;
    }

    // char* space;
    int i = 0;

    char* token = strtok(argcopy, " ");     //break argcopy into tokens using " " as a breaker

    char* redirectChar;
    
    
    while(token != NULL){

        if (i < ARGS_MAX)
        {
            // printf("args->args[%i]=%s, token=%s, TOKEN_MAX=%i\n", i, args->args[i], token, TOKEN_MAX);
            if (args->args[i] == NULL)
            {
                /* Malloc if needed */
                args->args[i] = (char*)malloc(TOKEN_MAX + 1);
                if (!args->args[i])
                {
                    perror("Error: Memory not available");
                    exit(1);
                }
            }

           
            strncpy(args->args[i++], token, TOKEN_MAX);
        }
        else
        {
            fprintf(stderr, "Error: too many process arguments\n");
            return -1;
        }
        // nextArg = space + 1;
        token = strtok(NULL, " ");
    }
    // strcpy(args[i], nextArg);
    if (args->args[i])
    {
        free(args->args[i]);
        args->args[i] = NULL;
    }

    free(argcopy);

    return 0;
}

void backgroundChildHandler()
{
    int saved_errno = errno;
    while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {}
    errno = saved_errno;
}

int findIndexOfPID(pid_t pidOfInterest , pid_t pids[], int numOfElements)
{
    for(int i = 0; i < numOfElements; i++)
    {
        if (pidOfInterest == pids[i])
            return i;
    }

    return -1;
}

void printMultipleCompletion(argParser* head, char* cmd, int pid_status[], pid_t pids[], int numOfElements)
{
    argParser* current = head;
    fprintf(stderr, "+ completed '%s' ", cmd);
    // for (int i = 0; i < numOfElements + 1; i++)
    // {
    //     fprintf(stderr, "[%i]", WEXITSTATUS(pid_status[i]));
    // }
    while (current != NULL)
    {
        int indexOfCurrent = findIndexOfPID(current->pid, pids, numOfElements);
        fprintf(stderr, "[%i]", pid_status[indexOfCurrent]);
        current = current->nextCmd;
    }
    fprintf(stderr, "\n");
}

void printCompletion(char* cmd, int retval)
{
    fprintf(stderr, "+ completed '%s' [%i]\n", cmd, retval);
}

void freeList(argParser** head)
{
    while(*head !=  NULL)
    {
        argParser* temp = *head;
        freeArgParser(temp);
        *head = (*head)->nextCmd;
        free(temp);
    }
}

int main(void)
{
        char cmd[CMDLINE_MAX];

        /* Create arguments array with maximum possible values */
        // char* args[ARGS_MAX + 1];
        // argParser args;
        // initArgParser(&args);
        struct argParser* head = NULL;

        // Signal handler setup for SIGCHLD
        struct sigaction sa;
        
        sa.sa_handler = &backgroundChildHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        if(sigaction(SIGCHLD, &sa, 0) == -1)
        {
            perror(0);
            exit(1);
        }



        while (1) {
                char* nl;
                int retval;

                /* Print prompt */
                printf("sshell@ucd$ ");
                fflush(stdout);

                /* Get command line */
                fgets(cmd, CMDLINE_MAX, stdin);

                /* Print command line if stdin is not provided by terminal */
                if (!isatty(STDIN_FILENO)) {
                    printf("%s", cmd);
                    fflush(stdout);
                }

                /* Remove trailing newline from command line */
                nl = strchr(cmd, '\n');
                if (nl)
                    *nl = '\0';

                

                checkRedirect(head, )

                int pipe_sign = scanArgs(&head, cmd);
                // printf("sdgfksdlkfsjd: %s\n", head->cmdPtr);
                // printf("NULL: %p", NULL);

                // struct argParser* current = head;
                // while(current != NULL)
                // {
                //     // Checking the code / Debugging 
                //     printf("Display List args\n============\n");
                //     // int i = 0;
                //     // while(args.args[i] != NULL)
                //     // {
                //     //     printf("Args[%i] = %s\n", i, args.args[i]);
                //     //     i++;
                //     // }
                //     printf("cmdptr: %s\n", current->cmdPtr);

                //     printf("File Name: %s\n", args.redirectFile);
                //     printf("====================\n");
                //     current = current->nextCmd;
                // }

                if(pipe_sign > 0)
                {
                    // printf("Number of commands:%d\n", pipe_sign);
                    struct argParser* current = head;
                    while (current != NULL)
                    {
                        if (makeArgs(current, current->cmdPtr))
                        {
                            /* Error checking when parsing the arguments*/
                            continue;
                        }
                        current = current->nextCmd;
                    }
                    
                    executePipeChain(head, pipe_sign + 1);
                    
                    int pid_status[pipe_sign];
                    pid_t pids[pipe_sign];
                    for(int i = 0; i < pipe_sign + 1; i++)
                    {
                        if (head->background)
                            pids[i] = waitpid(-1, &pid_status[i], WNOHANG);
                        else
                            pids[i] = waitpid(-1, &pid_status[i], 0);
                    }

                    printMultipleCompletion(head, cmd, pid_status, pids, pipe_sign+1);

                }
                else if (pipe_sign == 0)
                {
                    
                    if (makeArgs(head, head->cmdPtr))
                    {
                        /* Error Checking for processing arguments */
                        continue;
                    }

                    /* Builtin command */
                    /* Exit */
                    if (strcmp(head->args[0], "exit") == 0) {
                        fprintf(stderr, "Bye...\n");
                        retval = 0;
                        printCompletion(cmd, retval);
                        break;
                    }

                    /* pwd */
                    if (strcmp(head->args[0], "pwd") == 0)
                    {
                        char* cwd = (char*) malloc(PATH_MAX);
                        getcwd(cwd, PATH_MAX);

                        printf("%s\n", cwd);
                        retval = 0;
                        if (!cwd)
                        {
                            retval = 1;
                            free(cwd);
                        }
                        // fprintf(stderr, "+ completed %s [%i]\n", cmd, retval);
                        printCompletion(cmd, retval);
                        continue;
                    }

                    if (strcmp(head->args[0], "cd") == 0)
                    {
                        retval = chdir(head->args[1]);
                        if (retval)
                        {
                            /* If cd returns an error */
                            fprintf(stderr, "Error: cannot cd into directory\n");
                        }

                        printCompletion(cmd, retval);
                        continue;
                    }
                    

                    // Checking the code / Debugging 
                    // printf("Display args\n============\n");
                    // int i = 0;
                    // while(args.args[i] != NULL)
                    // {
                    //     printf("Args[%i] = %s\n", i, args.args[i]);
                    //     i++;
                    // }
                    // printf("File Name: %s\n", args.redirectFile);
                    // printf("====================\n");

                    pid_t pid = fork();
                    
                    if (pid == 0)
                    {
                        // Child
                        if (head->redirect)
                        {
                            // printf("REDIRECTING\n");
                            int fd;
                            if (head->isOutputAppend)
                                fd = open(head->redirectFile, O_WRONLY | O_CREAT | O_APPEND, 0644);
                            else
                                fd = open(head->redirectFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);   
                            //0644 file permissions; readable by all the user groups, but writable by the user only
                            dup2(fd, STDOUT_FILENO);
                            close(fd);
                        }

                        retval = execvp(head->args[0], head->args);
                        fprintf(stderr, "Error: command not found\n");
                        
                        exit(retval);
                    }
                    else if (pid > 0)
                    {
                        // Parent
                        if (head->background)
                            waitpid(pid, &retval, WNOHANG);
                        else
                            waitpid(pid, &retval, 0);
                        if (!WIFEXITED(retval))
                        {
                            printf("Error: child failed to exit\n");
                        }
                        // fprintf(stderr, "+ completed %s [%i]\n", cmd, retval);
                        printCompletion(cmd, WEXITSTATUS(retval));
                    }
                    else
                    {
                        perror("Error: unable to spawn child\n");
                        exit(1);
                    }

                    head->redirect = false;
                    head->background = false;
                    freeList(&head);
                    // /* Regular command */
                    // retval = system(cmd);
                    // fprintf(stdout, "Return status value for '%s': %d\n",
                    //         cmd, retval);
                }
        }

        freeArgParser(head);

        return EXIT_SUCCESS;
}
