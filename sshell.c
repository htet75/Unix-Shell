#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>

#define CMDLINE_MAX 512
#define ARGS_MAX 16
#define TOKEN_MAX 32
#define PATH_MAX 100

typedef struct argParser
{
    char* args[ARGS_MAX + 1];
    char* original;
    bool redirect;
    char* redirectFile;
    char* cmdPtr;

    struct argParser* nextCmd;

} argParser;

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

//https://www.learn-c.org/en/Linked_lists
void push(struct argParser ** head, char* argsString)
{
    struct argParser* new_node = malloc(sizeof(struct argParser));
    new_node->cmdPtr = argsString;
    new_node->nextCmd = NULL;

    if(*head == NULL)
    {
        *head = new_node;
    }
    else
    {
        struct argParser * end_node = *head;
        while(end_node->nextCmd != NULL)
        {
            end_node = end_node->nextCmd;
        }
        end_node->nextCmd = new_node;
    }
}

int scanArgs(struct argParser ** args, char* argsString)
{
    char* argcopy = (char*)malloc(CMDLINE_MAX + 1);
    strcpy (argcopy, argsString);
    char* token = strtok(argcopy, "|");
    *args = NULL;

    int pipeCount = -1;

    while(token != NULL)
    {
        pipeCount++;
        push(args, token);
        token = strtok(NULL, "|");
    }
    // print_list(args);
    return pipeCount;
}

int makeArgs(argParser* args, char* argsString)
{
    // printf("Making Args\n");
    char* argcopy = (char*)malloc(CMDLINE_MAX + 1);
    
    strcpy(argcopy, argsString);

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
                // printf("Char by itself\n");
                fileName = strtok(NULL, " ");
                strncpy(args->redirectFile, fileName, TOKEN_MAX);
                token = strtok(NULL, " ");
                continue;
            }
            else if (*(redirectChar + 1) == '\0')
            {
                /* Redirect Char at end of word */
                *redirectChar = '\0';
                printf("Char at end of word\n");
                fileName = strtok(NULL, " ");
                strncpy(args->redirectFile, fileName, TOKEN_MAX);
            }
            else if (redirectChar == token)
            {
                /* Redirect Char at beginning of word */
                printf("Char at beginning of word\n");
                token = token + 1;
                strncpy(args->redirectFile, token, TOKEN_MAX);
                token = strtok(NULL, " ");
                continue;
            }
            else 
            {
                /* Redirect Char in middle of word */
                printf("Char in middle of word\n");
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

void printCompletion(char* cmd, int retval)
{
    fprintf(stderr, "+ completed %s [%i]\n", cmd, retval);
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
                char *nl;
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

                struct argParser * head = NULL;

                int pipe_sign = scanArgs(&head, cmd);

                if(pipe_sign > 0)
                {
                    printf("Number of commands:%d\n", pipe_sign);
                    if(makeArgs(&parse1, head->cmdPtr))
                    {
                        continue;
                    }
                    printf("Command1: %s\n", parse1.args[0]);
                    if(makeArgs(&parse2, head->nextCmd->cmdPtr))
                    {
                        continue;
                    }
                    printf("Command2: %s\n", parse2.args[0]);
                    if(pipe_sign == 2)
                    {
                        if(makeArgs(&args, head->nextCmd->nextCmd->cmdPtr))
                        {
                            continue;
                        }
                        printf("Command3: %s\n", args.args[0]);
                    }
                }

                if(pipe_sign == 2)
                {
                    // parse1 = command1; parse2 = command2; args = command3;

                    int fd_1t2[2], fd_2t3[2];
                    pipe(fd_1t2); pipe(fd_2t3);

                    pid_t pid[3];
                    int pid_status[3];
                    for(int i = 0; i < 3; i++)
                    {
                        pid_status[i] = 0;
                    }
                    
                    pid[0] = fork();
                    if(pid[0] == 0)         //child 1 for command 1 to command 2
                    {
                        dup2(fd_1t2[1], STDOUT_FILENO);
                        for(int i = 0; i < 2; i++)
                        {
                            close(fd_1t2[i]);
                            close(fd_2t3[i]);
                        }
                        pid_status[0] = execvp(parse1.args[0], parse1.args);
                        fprintf(stderr, "Command1 not found\n");
                        exit(0);
                    }
                    pid[1] = fork();
                    if(pid[1] == 0)     //child 2 for command 2 to 3
                    {
                        dup2(fd_1t2[0], STDIN_FILENO);         //command 2 reads from command 1
                        dup2(fd_2t3[1], STDOUT_FILENO);        //command 2 writes to command 3
                        for(int i = 0; i < 2; i++)
                        {
                            close(fd_1t2[i]);
                            close(fd_2t3[i]);
                        }
                        pid_status[1] = execvp(parse2.args[0], parse2.args);
                        fprintf(stderr, "Command2 not found\n");
                        exit(0);
                    }
                    pid[2] = fork();
                    if(pid[2] == 0)
                    {
                        dup2(fd_2t3[0], STDIN_FILENO);      //command3 reads from command 2
                        for(int i = 0; i < 2; i++)
                        {
                            close(fd_1t2[i]);
                            close(fd_2t3[i]);
                        }
                        pid_status[2] = execvp(args.args[0], args.args);
                        fprintf(stderr, "Command3 not found\n");
                        exit(0);
                    }
                    for(int i = 0; i < 2; i++)
                    {
                        close(fd_1t2[i]);
                        close(fd_2t3[i]);
                    }
                    for(int i = 0; i < 3; i++)
                    {
                        int status;
                        waitpid(pid[i], &status, 0);
                        printf("Child %d exited with status [%d]\n", i, pid_status[i]);
                    }
                    printCompletion(cmd, pid_status[0]);
                }
                
                if(pipe_sign == 1)
                {
                    int fd[2];
                    if(pipe(fd) < 0)
                    {
                        fprintf(stderr, "Pipe could not be initiated\n");
                    }

                    pid_t p1, p2;
                    p1 = fork();
                    if(p1 < 0 )
                    {
                        printf("child 1 fork failed\n");
                    }

                    if( p1 == 0 )
                    {
                        close(fd[0]);
                        dup2(fd[1], STDOUT_FILENO);
                        close(fd[1]);
                        retval = execvp(parse1.args[0], parse1.args);
                        if(retval < 0 )
                        {
                            printf("execvp failed \n");
                            exit(0);
                        }
                        fprintf(stderr, "Error: command not found\n");
                    
                        exit(retval);
                    }
                    else
                    {
                        p2 = fork();
                        if(p2 < 0)
                        {
                            printf("child 2 fork failed\n");
                        }
                        if(p2 == 0)
                        {
                            close(fd[1]);
                            dup2(fd[0], STDIN_FILENO);
                            close(fd[0]);
                            retval = execvp(parse2.args[0], parse2.args);
                            if(retval < 0)
                            {
                                printf("execvp 2 failed\n");
                                exit(0);
                            }
                            fprintf(stderr, "Error: command 2 not found\n");

                            exit(retval);
                        }
                        else
                        {
                            wait(NULL);
                            wait(NULL);
                            printCompletion(cmd, retval);
                        }
                    }
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
                    

                    //Checking the code / Debugging 
                    // printf("Display args\n============\n");
                    // while(args.args[args.num])
                    // {
                    //     printf("Args[%i] = %s\n", args.num, args.args[args.num]);
                    //     args.num++;
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
                            int fd = open(args.redirectFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);   
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
                        printCompletion(cmd, retval);
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
