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
    int num;
    char* args[ARGS_MAX + 1];
    char* original;
    bool redirect;
    char* redirectFile;


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

int makeArgs(argParser* args, char* argsString)
{
    // printf("Making Args\n");
    char* argcopy = (char*)malloc(CMDLINE_MAX + 1);
    
    strcpy(argcopy, argsString);

    // char* space;
    int i = 0;

    char* token = strtok(argcopy, " ");

    char* redirectChar;
    

    while(token != NULL){
        // printf("Token: %s\n", token);
        redirectChar = strchr(token, '>');
        if (redirectChar != NULL)
        {
            args->redirect = true;  
            // char* fileName = (char*)malloc(TOKEN_MAX + 1);
            char* fileName;
            if (strlen(token) == 1)
            {
                /* Redirect char by itself*/
                printf("Char by itself\n");
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

                if (makeArgs(&args, cmd))
                {
                    /* Error Checking for processing arguments */
                    continue;
                }

                args.num = 0;

                if (args.redirect)

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
                
                printf("Display args\n============\n");
                while(args.args[args.num])
                {
                    printf("Args[%i] = %s\n", args.num, args.args[args.num]);
                    args.num++;
                }
                printf("File Name: %s\n", args.redirectFile);
                printf("====================\n");


                pid_t pid = fork();
                
                if (pid == 0)
                {
                    // Child
                    if (args.redirect)
                    {
                        // printf("REDIRECTING\n");
                        int fd = open(args.redirectFile, O_WRONLY | O_CREAT | O_TRUNC);
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

        freeArgParser(&args);

        return EXIT_SUCCESS;
}
