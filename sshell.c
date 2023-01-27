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
    char* original;
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

    // int fd_1t2[2], fd_2t3[2];
    // pipe(fd_1t2); pipe(fd_2t3);
    
    // pid_t pid[3];

    // for(int i = 0; i < numPipeCommands; i++)
    // {
    //     pid[i] = fork();
    //     if (pid[i] == 0)
    //     {
    //         if (i == 0)
    //         {
    //             /* Child 1 for command 1 to command 2 */
    //             dup2(fd_1t2[1], STDOUT_FILENO);
    //             for(int i = 0; i < 2; i++)
    //             {
    //                 close(fd_1t2[i]);
    //                 close(fd_2t3[i]);
    //             }
    //             execvp(head->args[0], head->args);
    //             fprintf(stderr, "Command1 not found\n");
    //             exit(0);
    //         }
    //         else if (i == 1)
    //         {
    //             dup2(fd_1t2[0], STDIN_FILENO);         //command 2 reads from command 1
    //             dup2(fd_2t3[1], STDOUT_FILENO);        //command 2 writes to command 3
    //             for(int i = 0; i < 2; i++)
    //             {
    //                 close(fd_1t2[i]);
    //                 close(fd_2t3[i]);
    //             }
    //             execvp(head->nextCmd->args[0], head->nextCmd->args);
    //             fprintf(stderr, "Command2 not found\n");
    //             exit(0);
    //         }
    //         else if (i == 2)
    //         {
    //             dup2(fd_2t3[0], STDIN_FILENO);      //command3 reads from command 2
    //             for(int i = 0; i < 2; i++)
    //             {
    //                 close(fd_1t2[i]);
    //                 close(fd_2t3[i]);
    //             }
    //             execvp(head->nextCmd->nextCmd->args[0], head->nextCmd->nextCmd->args);
    //             fprintf(stderr, "Command3 not found\n");
    //             exit(0);
    //         }
    //         else
    //             printf("ERROR\n");
            
    //     }
    // }


    // for(int i = 0; i < 2; i++)
    // {
    //     close(fd_1t2[i]);
    //     close(fd_2t3[i]);
    // }




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
                dup2(pipes[0][0], STDIN_FILENO);         //command 2 reads from command 1
                dup2(pipes[1][1], STDOUT_FILENO);        //command 2 writes to command 3
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
    new_node->cmdPtr = argsString;
    new_node->nextCmd = NULL;
    
    if(*head == NULL)
    {
        *head = new_node;
    }
    else
    {
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
    
    return pipeCount;
}

void checkOutputAppending(argParser* args, char* argString)
{
    char* outputAppendPosition = strstr(argString, ">>");
    
    if (outputAppendPosition == NULL)
        /* Didn't find output appending */
        return;
    
    *(outputAppendPosition++) = '\0';
    *(outputAppendPosition++) = '\0';
    
    while (*outputAppendPosition == ' ')
        outputAppendPosition++;
    args->redirect = true;
    args->isOutputAppend = true;
    strcpy(args->redirectFile, outputAppendPosition);
}

int makeArgs(argParser* args, char* argsString)
{
    // printf("Making Args\n");
    char* argcopy = (char*)malloc(CMDLINE_MAX + 1);
    
    strcpy(argcopy, argsString);

    checkOutputAppending(args, argcopy);

    // char* space;
    int i = 0;

    char* token = strtok(argcopy, " ");     //break argcopy into tokens using " " as a breaker

    char* redirectChar;
    
    
    while(token != NULL){
        // printf("Token: %s\n", token);
        redirectChar = strchr(token, '>');      // redirectChar = string after '>' from token
        if (redirectChar != NULL)               // if there is an existing token after '>' 
        {
            args->redirect = true;  
            // char* fileName = (char*)malloc(TOKEN_MAX + 1);
            char* fileName;
            if (strlen(token) == 1)
            {
                /* Redirect char by itself*/
                fileName = strtok(NULL, " ");
                strncpy(args->redirectFile, fileName, TOKEN_MAX);
                token = strtok(NULL, " ");
                continue;
            }
            else if (*(redirectChar + 1) == '\0')
            {
                /* Redirect Char at end of word */
                *redirectChar = '\0';
                fileName = strtok(NULL, " ");
                strncpy(args->redirectFile, fileName, TOKEN_MAX);
            }
            else if (redirectChar == token)
            {
                /* Redirect Char at beginning of word */
                token = token + 1;
                strncpy(args->redirectFile, token, TOKEN_MAX);
                token = strtok(NULL, " ");
                continue;
            }
            else 
            {
                /* Redirect Char in middle of word */
                *redirectChar = '\0';
                redirectChar++;
                // if (args->args[i] == NULL)
                // {
                //     /* Malloc if needed */
                //     args->args[i] = (char*)malloc(TOKEN_MAX + 1);
                //     if (!args->args[i])
                //     {
                //         perror("Error: Memory not available");
                //         exit(1);
                //     }
                // }
                // strncpy(args->args[i++], token, TOKEN_MAX);
                strncpy(args->redirectFile, redirectChar, TOKEN_MAX);
            }
        }


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

           if(strchr(token, '&') != NULL)
            {
                char * newtoken = strtok(token, "&");
                args->background = true;
                if(newtoken != NULL)
                {
                    strncpy(args->args[i++], newtoken, TOKEN_MAX);
                }
            }
            else
            {
                strncpy(args->args[i++], token, TOKEN_MAX);
            }
            // strncpy(args->args[i++], token, TOKEN_MAX);
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

void zombie_hunter()
{
    int pid, status, serrno;
    serrno = errno;
    while(1)
    {
        pid = waitpid( WAIT_ANY, &status, WNOHANG);
        if(pid < 0)
        {
            perror("waitpid");
            break;
        }
        if(pid == 0)
            break;
        printf("Child %d terminated with status [%d]\n", pid, WEXITSTATUS(status));
    }
    errno = serrno;
}

void printCompletion(char* cmd, int retval)
{
    fprintf(stderr, "+ completed '%s' [%i]\n", cmd, retval);
}

int main(void)
{
        char cmd[CMDLINE_MAX];

        /* Create arguments array with maximum possible values */
        // char* args[ARGS_MAX + 1];
        argParser args;
        initArgParser(&args);

        argParser parse1;
        initArgParser(&parse1);

        argParser parse2;
        initArgParser(&parse2);

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

                args.original = cmd;

                struct argParser* head = NULL;
                int pipe_sign = scanArgs(&head, cmd);
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
                    
                    // if(makeArgs(&parse1, head->cmdPtr))
                    // {
                    //     /*Error checking*/
                    //     continue;
                    // }
                    // // printf("Command1: %s\n", parse1.args[0]);
                    // if(makeArgs(&parse2, head->nextCmd->cmdPtr))
                    // {
                    //     continue;
                    // }
                    // printf("Command2: %s\n", parse2.args[0]);
                    // if(pipe_sign == 2)
                    // {
                    //     if(makeArgs(&args, head->nextCmd->nextCmd->cmdPtr))
                    //     {
                    //         continue;
                    //     }
                    //     // printf("Command3: %s\n", args.args[0]);
                    // }
                }

                if(pipe_sign == 2)
                {
                    // parse1 = command1; parse2 = command2; args = command3;

                    // int fd_1t2[2], fd_2t3[2];
                    // pipe(fd_1t2); pipe(fd_2t3);

                    // pid_t pid[3];
                    
                    
                    
                    // pid[0] = fork();
                    // if(pid[0] == 0)
                    // {
                    //     /* Child 1 for command 1 to command 2 */
                    //     dup2(fd_1t2[1], STDOUT_FILENO);
                    //     for(int i = 0; i < 2; i++)
                    //     {
                    //         close(fd_1t2[i]);
                    //         close(fd_2t3[i]);
                    //     }
                    //     execvp(head->args[0], head->args);
                    //     fprintf(stderr, "Command1 not found\n");
                    //     exit(0);
                    // }
                    // pid[1] = fork();
                    // if(pid[1] == 0)     //child 2 for command 2 to 3
                    // {
                    //     dup2(fd_1t2[0], STDIN_FILENO);         //command 2 reads from command 1
                    //     dup2(fd_2t3[1], STDOUT_FILENO);        //command 2 writes to command 3
                    //     for(int i = 0; i < 2; i++)
                    //     {
                    //         close(fd_1t2[i]);
                    //         close(fd_2t3[i]);
                    //     }
                    //     execvp(head->nextCmd->args[0], head->nextCmd->args);
                    //     fprintf(stderr, "Command2 not found\n");
                    //     exit(0);
                    // }
                    // pid[2] = fork();
                    // if(pid[2] == 0)
                    // {
                    //     dup2(fd_2t3[0], STDIN_FILENO);      //command3 reads from command 2
                    //     for(int i = 0; i < 2; i++)
                    //     {
                    //         close(fd_1t2[i]);
                    //         close(fd_2t3[i]);
                    //     }
                    //     execvp(head->nextCmd->nextCmd->args[0], head->nextCmd->nextCmd->args);
                    //     fprintf(stderr, "Command3 not found\n");
                    //     exit(0);
                    // }
                    // for(int i = 0; i < 2; i++)
                    // {
                    //     close(fd_1t2[i]);
                    //     close(fd_2t3[i]);
                    // }
                    
                    int pid_status[3];
                    for(int i = 0; i < 3; i++)
                    {
                        pid_status[i] = 0;
                    }


                    executePipeChain(head, 3);
                    waitpid(-1, &pid_status[0], 0);
                    waitpid(-1, &pid_status[1], 0);
                    waitpid(-1, &pid_status[2], 0);

                    // waitpid(head->pid, &pid_status[0], 0);
                    // waitpid(head->nextCmd->pid, &pid_status[1], 0);
                    // waitpid(head->nextCmd->nextCmd->pid, &pid_status[2], 0);
                    // for (int i = 0; i < pipe_sign + 1; i++)
                    // {
                    //     waitpid(pid[i], &pid_status[i], 0);
                    // }
                    // bool isChildSuccess = true;
                    // for (int i = 0; i < pipe_sign + 1; i++)
                    // {
                    //     if (!WIFEXITED(pid_status[i]))
                    //     {
                    //         /* Child exited unsuccessfully */
                    //         fprintf(stderr, "Child[%d] exited unsuccessful\n", i);
                    //         isChildSuccess = false;
                    //     }
                    // }
                    
                    // if (!isChildSuccess)
                    //     continue;
                        

                    fprintf(stderr, "+ completed '%s' ", cmd);
                    for (int i = 0; i < pipe_sign + 1; i++)
                    {
                        fprintf(stderr, "[%i]", WEXITSTATUS(pid_status[i]));
                    }
                    fprintf(stderr, "\n");
                }
                
                if(pipe_sign == 1)
                {
                    int fd[2];

                    int retVals[2];

                    if(pipe(fd) < 0)
                    {
                        fprintf(stderr, "Pipe could not be initiated\n");
                    }

                    // executePipeChain(head, 2);

                    makePipe(&parse1, &parse2);

                    waitpid(parse1.pid, &retVals[0], 0);
                    waitpid(parse2.pid, &retVals[1], 0);

                    fprintf(stderr, "+ completed %s [%i][%i]\n", cmd, WEXITSTATUS(retVals[0]), WEXITSTATUS(retVals[1]));
                }
                if(pipe_sign == 0)
                {
                    if (makeArgs(&args, cmd))
                    {
                        /* Error Checking for processing arguments */
                        printf("Error checking arguments\n");
                        continue;
                    }

                    /* Builtin command */
                    /* Exit */
                    if (strcmp(args.args[0], "exit") == 0) {
                        fprintf(stderr, "Bye...\n");
                        retval = 0;
                        printCompletion(cmd, retval);
                        break;
                    }

                    /* pwd */
                    if (strcmp(args.args[0], "pwd") == 0)
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

                    if (strcmp(args.args[0], "cd") == 0)
                    {
                        retval = chdir(args.args[1]);
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
                        if (args.redirect)
                        {
                            // printf("REDIRECTING\n");
                            int fd;
                            if (args.isOutputAppend)
                                fd = open(args.redirectFile, O_WRONLY | O_CREAT | O_APPEND, 0644);
                            else
                                fd = open(args.redirectFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);   
                            //0644 file permissions; readable by all the user groups, but writable by the user only
                            dup2(fd, STDOUT_FILENO);
                            close(fd);
                        }

                        retval = execvp(args.args[0], args.args);
                        fprintf(stderr, "Error: command not found\n");
                        
                        exit(retval);
                    }
                    else if (pid > 0)
                    {
                        // Parent
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

                    args.redirect = false;
                    // /* Regular command */
                    // retval = system(cmd);
                    // fprintf(stdout, "Return status value for '%s': %d\n",
                    //         cmd, retval);
                }
        }

        freeArgParser(&args);

        return EXIT_SUCCESS;
}
