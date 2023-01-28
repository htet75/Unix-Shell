#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* Constants to define limitations of our simple shell */
#define CMDLINE_MAX 512
#define ARGS_MAX 16
#define TOKEN_MAX 32
#define PATH_MAX 100
#define PIPE_OUTPUT 2
#define LIBRARY_ERROR 1


typedef struct argParser
{
    /* Object to contain information about executable programs and their arguments */
    char* args[ARGS_MAX + 1];
    char* cmdPtr;
    pid_t pid;
    struct argParser* nextCmd;

} argParser;

typedef struct redirectInfo
{
    /* Variables associated with features available to the command line interpreter */
    char* redirectFile;
    bool redirect;
    bool isOutputAppend;
    bool background;

} redirectInfo;

typedef struct job
{
    /* A linked list containing information for background jobs */
    struct job *next;
    char *command;
    pid_t pid;

} job;

void executeParser(argParser* parser)
{
    execvp(parser->args[0], parser->args);
    fprintf(stderr, "Failed to execute command '%s'\n", parser->args[0]);
    exit(LIBRARY_ERROR);
}

void closeAllPipes(int pipes[][PIPE_OUTPUT], int numPipeCommands)
{
    for (int j = 0; j < numPipeCommands - 1; j++)
    {
        close(pipes[j][0]);
        close(pipes[j][1]);
    }
}

void executePipeChain(argParser* head, int numPipeCommands, struct redirectInfo* redirectionInfo)
{
    /* Create enough pipes for the command line given*/
    int pipes[numPipeCommands - 1][PIPE_OUTPUT];
    for (int i = 0; i < numPipeCommands - 1; i++)
    {
        if(pipe(pipes[i]) < 0)
        {
            fprintf(stderr, "Error: unable to make pipe\n");
            exit(EXIT_FAILURE);
        }
    }

    argParser* current = head;
    for(int i = 0; i < numPipeCommands; i++)
    {
        /* Create multiple children for each command in the shell */
        current->pid = fork();
        if (current->pid < 0)
        {
            /* Error checking for failure of child fork */
            fprintf(stderr, "Child fork failed\n");
            exit(LIBRARY_ERROR);
        }
        else if (current->pid == 0)
        {
            /* Child Successfully forked */
            if (i == 0)
            {
                /* If first command in pipeline */
                dup2(pipes[0][1], STDOUT_FILENO);
                closeAllPipes(pipes, numPipeCommands);
            }
            else if (i == numPipeCommands - 1)
            {
                /* If last command in pipeline */
                dup2(pipes[i-1][0], STDIN_FILENO);
                
                /* Handle redirect if necessary */
                if (redirectionInfo->redirect)
                {
                    int fd_redirect;
                    if (redirectionInfo->isOutputAppend)
                        fd_redirect = open(redirectionInfo->redirectFile, O_WRONLY | O_CREAT | O_APPEND, 0644);
                    else
                        fd_redirect = open(redirectionInfo->redirectFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    
                    dup2(fd_redirect, STDOUT_FILENO);
                }

                /* Close out all the file descriptors in the parent function*/
                closeAllPipes(pipes, numPipeCommands);
            }
            else
            {
                dup2(pipes[i-1][0], STDIN_FILENO);         //command 2 reads from command 1
                dup2(pipes[i][1], STDOUT_FILENO);        //command 2 writes to command 3
                closeAllPipes(pipes, numPipeCommands);
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

void initArgParser(argParser* args)
{
    for (int i = 0; i < ARGS_MAX + 1; i++)
    {
        args->args[i] = NULL;
    }

}

void freeArgParser(argParser* args)
{
    for (int i = 0; i < ARGS_MAX + 1; i++)
    {
        if(args->args[i] != NULL)
            free(args->args[i]);
    }
    free(args->cmdPtr);
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

bool isWordOnlySpaces(char* str)
{
    for (size_t i = 0; i < strlen(str); i++)
    {
        if (!isspace(str[i]))
        {
            return false;
        }
    }
    return true;
}

int scanArgs(struct argParser** args, char* argsString)
{
    /* Going through every character in argsString and check if '|' exists*/
    /* keep counts of the number of the symbol '|' and number of comments*/
    char* argcopy = (char*)malloc(CMDLINE_MAX + 1);
    strcpy (argcopy, argsString);
    
    int i = 0;
    int pipeCharCount = 0;
    while(argsString[i] != '\0')
    {
        if (argsString[i] == '|')
        {
            pipeCharCount++;
        }
        i++;
    }
    
    char* token = strtok(argcopy, "|");

    int pipeCount = -1; // Offset of one to account for pipe instead of tokens
    while(token != NULL)
    {
        if (!isWordOnlySpaces(token))
        {
            pipeCount++;
            push(args, token);
        }
        token = strtok(NULL, "|");
    }
    if (pipeCharCount != pipeCount)
    {
        fprintf(stderr, "Error: missing command\n");
        return -1;
    }

    free(argcopy);
    return pipeCount;
}

int checkRedirect(struct redirectInfo* redirectinfo, char* cmd)
{
    char* redirectChar = strchr(cmd, '>');      // redirectChar = string after '>' from token
    if (redirectChar != NULL)               // if there is an existing token after '>' 
    {
        
        // Get the word after the output redirection '>' 
        char* token = strtok(cmd, ">");
        if (redirectChar < token)
        {
            /* Check if the output redirection is the earliest split */
            fprintf(stderr, "Error: missing command\n");
            return -1;
        }
        token = strtok(NULL, ">");
        
        if (token == NULL)
        {
            fprintf(stderr, "Error: no output file\n");
            return -1;
        }
        while (*token == ' ') // To get rid of leading white space if any
            token++;

        redirectinfo->redirect = true;
        
        char* fileName;
        fileName = strtok(token, " ");
        strncpy(redirectinfo->redirectFile, fileName, TOKEN_MAX);

        token = strtok(NULL, " ");
        if (token != NULL)
        {
            fprintf(stderr, "Error: mislocated output redirection\n");
            return -1;
        }
    }
    return EXIT_SUCCESS;
}

int checkOutputAppending(struct redirectInfo* redirectinfo, char* argString)
{
    char* outputAppendPosition = strstr(argString, ">>");
    if (outputAppendPosition == NULL)
        /* Didn't find output appending */
        return EXIT_SUCCESS;
    
    *(outputAppendPosition++) = '\0';
    *(outputAppendPosition++) = '\0';

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

    redirectinfo->redirect = true;
    redirectinfo->isOutputAppend = true;
    strcpy(redirectinfo->redirectFile, outputAppendPosition);

    return EXIT_SUCCESS;
}

int checkBackground(struct redirectInfo* redirectionInfo, char* argsString)
{
    /* check for '&' in the command and update the status of the child processs accordingly*/
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
        redirectionInfo->background = true;
    }

    return EXIT_SUCCESS;
}

int makeArgs(argParser* args, char* argsString)
{
    char* argcopy = (char*)malloc(CMDLINE_MAX + 1);
    
    strcpy(argcopy, argsString);

    int i = 0;
    char* token = strtok(argcopy, " ");     //break argcopy into tokens using " " as a breaker
    
    while(token != NULL){

        if (i < ARGS_MAX)
        {
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
        token = strtok(NULL, " ");
    }
    if (args->args[i])
    {
        free(args->args[i]);
        args->args[i] = NULL;
    }

    free(argcopy);

    return EXIT_SUCCESS;
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
    while (current != NULL)
    {
        int indexOfCurrent = findIndexOfPID(current->pid, pids, numOfElements);
        fprintf(stderr, "[%i]", WEXITSTATUS(pid_status[indexOfCurrent]));
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
    argParser* newHead = *head;
    while(newHead != NULL)
    {
        argParser* deleteNode = newHead;
        newHead = newHead->nextCmd;
        freeArgParser(deleteNode);
        free(deleteNode);
    }
}

void pushJob(job** jobhead, pid_t pid, char* command)
{
    job* newJob = (job*)malloc(sizeof(job));
    newJob->next = NULL;
    newJob->pid = pid;
    newJob->command = (char*)malloc(sizeof(char) * TOKEN_MAX);
    strcpy(newJob->command, command);

    if (*jobhead == NULL)
    {
        *jobhead = newJob;
    }
    else
    {
        // Add new job to the back of the list
        job* endJob = *jobhead;
        while(endJob->next != NULL)
        {
            endJob = endJob->next;
        }
        endJob->next = newJob;
    }
}

void freeJob(job** jobhead, pid_t pid)
{
    /* Remove the specific pid job from the list */
    job* previousJob = *jobhead;
    job* currentJob = *jobhead;
    while(currentJob != NULL)
    {
        if(currentJob->pid == pid)
        {
            if(currentJob == *jobhead)
            {
                *jobhead = (*jobhead)->next;
                free(currentJob->command);
                free(currentJob);
                break;
            }
            else
            {
                previousJob->next = currentJob->next;
                free(currentJob->command);
                free(currentJob);
                break;
            }
        }
        else
        {
            previousJob = currentJob;
            currentJob = currentJob->next;
        }
    }
}

void printBackgroundProcess(job** joblist)
{
    /* 
    Specific print function to check the background processes and output the values
       if finished
    */
    job* currentJob = *joblist;
    while (currentJob != NULL)
    {
        int returnValue;
        pid_t pid = waitpid(currentJob->pid, &returnValue, WNOHANG);                                
        if (pid == currentJob->pid)
        {
            printCompletion(currentJob->command, WEXITSTATUS(returnValue));
            freeJob(joblist, currentJob->pid);
        }
        currentJob = currentJob->next;
    } 
}

int main(void)
{
        char cmd[CMDLINE_MAX];

        job* joblist = NULL;

        struct argParser* head = NULL;
        struct redirectInfo redirectionInfo;

        redirectionInfo.redirectFile = (char*)malloc(sizeof(char) * CMDLINE_MAX);
        redirectionInfo.isOutputAppend = false;
        redirectionInfo.redirect = false;

        while (1) {
                char* nl;
                int retval;

                /* Print prompt */
                printf("sshell@ucd$ ");
                fflush(stdout);

                /* Get command line */
                if (!fgets(cmd, CMDLINE_MAX, stdin))
                {
                    fprintf(stderr, "Error: fgets\n");
                    exit(EXIT_FAILURE);
                }

                /* Print command line if stdin is not provided by terminal */
                if (!isatty(STDIN_FILENO)) {
                    printf("%s", cmd);
                    fflush(stdout);
                }

                /* Remove trailing newline from command line */
                nl = strchr(cmd, '\n');
                if (nl)
                    *nl = '\0';

                char* copyCmd = (char*)malloc(sizeof(char) * CMDLINE_MAX);
                strcpy(copyCmd, cmd);

                /* Initialize the redirectionInfo to false to avoid false positives */
                redirectionInfo.redirect = false;
                redirectionInfo.background = false;
                redirectionInfo.isOutputAppend = false;

                /* Check the special methods supported by our shell */
                if (checkBackground(&redirectionInfo, cmd) || 
                    checkOutputAppending(&redirectionInfo, cmd) || 
                    checkRedirect(&redirectionInfo, cmd)) 
                {
                    redirectionInfo.redirect = false;
                    redirectionInfo.isOutputAppend = false;
                    head = NULL;
                    continue;
                }

                /* Get Pipe number */
                int numPipes = scanArgs(&head, cmd);
                if (numPipes < 0)
                {
                    /* Error checking for scanArgs */
                    redirectionInfo.redirect = false;
                    redirectionInfo.isOutputAppend = false;
                    if (head != NULL)
                        freeList(&head);
                    head = NULL;
                    continue;
                }

                if(numPipes > 0)
                {
                    /* There is a pipe in the command line */
                    struct argParser* current = head;
                    while (current != NULL)
                    {
                        if (makeArgs(current, current->cmdPtr))
                        {
                            /* Error checking when parsing the arguments*/
                            redirectionInfo.redirect = false;
                            redirectionInfo.isOutputAppend = false;
                            redirectionInfo.background = false;
                            freeList(&head);
                            head = NULL;
                            continue;
                        }
                        current = current->nextCmd;
                    }
                    
                    executePipeChain(head, numPipes + 1, &redirectionInfo);
                    
                    int pid_status[numPipes];
                    pid_t pids[numPipes];
                    for(int i = 0; i < numPipes + 1; i++)
                    {
                        if (redirectionInfo.background)
                            pids[i] = waitpid(-1, &pid_status[i], WNOHANG);
                        else
                            pids[i] = waitpid(-1, &pid_status[i], 0);
                    }

                    printMultipleCompletion(head, copyCmd, pid_status, pids, numPipes+1);
                    
                    redirectionInfo.redirect = false;
                    redirectionInfo.isOutputAppend = false;
                    redirectionInfo.background = false;
                    freeList(&head);
                    head = NULL;
                }
                else if (numPipes == 0)
                {
                    if (makeArgs(head, head->cmdPtr))
                    {
                        /* Error Checking for processing arguments */
                        redirectionInfo.redirect = false;
                        redirectionInfo.isOutputAppend = false;
                        redirectionInfo.background = false;
                        freeList(&head);
                        head = NULL;
                        continue;
                    }

                    /* Builtin command */
                    /* Exit */
                    if (strcmp(head->args[0], "exit") == 0) {
                        fprintf(stderr, "Bye...\n");
                        retval = 0;
                        if (joblist != NULL)
                        {
                            fprintf(stderr, "Error: active jobs still running\n");
                        }
                        printCompletion(cmd, retval);
                        if (joblist == NULL)
                            exit(EXIT_SUCCESS);
                        continue;
                    }

                    /* pwd */
                    if (strcmp(head->args[0], "pwd") == 0)
                    {
                        char* cwd = (char*) malloc(PATH_MAX);
                        if(!getcwd(cwd, PATH_MAX))
                        {
                            fprintf(stderr, "Error: can't get current directory\n");
                            exit(EXIT_FAILURE);
                        }

                        printf("%s\n", cwd);
                        retval = 0;
                        if (!cwd)
                        {
                            retval = 1;
                            free(cwd);
                        }
                        // fprintf(stderr, "+ completed %s [%i]\n", cmd, retval);
                        printCompletion(copyCmd, retval);
                    }
                    else if (strcmp(head->args[0], "cd") == 0)
                    {
                        retval = chdir(head->args[1]);
                        if (retval)
                        {
                            /* If cd returns an error */
                            fprintf(stderr, "Error: cannot cd into directory\n");
                        }

                        printCompletion(copyCmd, retval);
                    }
                    else
                    {
                        pid_t pid = fork();
                    
                        if (pid == 0)
                        {
                            // Child
                            if (redirectionInfo.redirect)
                            {
                                int fd;
                                // 0644 file permissions; readable by all the user groups, but writable by the user only
                                if (redirectionInfo.isOutputAppend)
                                    fd = open(redirectionInfo.redirectFile, O_WRONLY | O_CREAT | O_APPEND, 0644);
                                else
                                    fd = open(redirectionInfo.redirectFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

                                if (fd < 0)
                                {
                                    fprintf(stderr, "Error: cannot open output file\n");
                                    exit(EACCES);
                                }
                                else
                                {
                                    
                                    dup2(fd, STDOUT_FILENO);
                                    close(fd);
                                }
                            }
                                

                            retval = execvp(head->args[0], head->args);
                            fprintf(stderr, "Error: command not found\n");
                            
                            exit(retval);
                        }
                        else if (pid > 0)
                        {
                            // Parent

                            // printJobList(joblist);

                            if (redirectionInfo.background)
                            {
                                waitpid(pid, &retval, WNOHANG);
                                pushJob(&joblist, pid, copyCmd);
                                printBackgroundProcess(&joblist);
                                
                                // printf("Added to job list\n");
                            }
                            else
                            {
                                waitpid(pid, &retval, 0);
                                if (!WIFEXITED(retval))
                                {
                                    printf("Error: child failed to exit\n");
                                }

                                printBackgroundProcess(&joblist);
                                if(WEXITSTATUS(retval) != EACCES)
                                {
                                    printCompletion(copyCmd, WEXITSTATUS(retval));
                                }
                            }
                            
                        }
                        else
                        {
                            perror("Error: unable to spawn child\n");
                            exit(1);
                        }
                    }

                    redirectionInfo.redirect = false;
                    redirectionInfo.isOutputAppend = false;
                    redirectionInfo.background = false;
                    freeList(&head);
                    head = NULL;
                }
        }

        freeArgParser(head);

        return EXIT_SUCCESS;
}
