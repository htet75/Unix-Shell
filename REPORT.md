# sshell: Simple Shell

## Introduction
This project is a simple shell called **sshell** implemented using system calls
to execute inputs from the command line.

## Implementation

### Data Structure
Initially the `sshell` input is processed into a data structure called 
`struct argParser` which parses the initial input. This data structure is a list
containning information relating to the arguments such as the commands and their
arguments. There is another data structure called `struct redirectInfo` which also 
processes the input and extracts information relating to recirecting and ouput
appending. The third data structure called `struct job` store the information
of all the child processes that are concurrently running in the background. We
also used all three of these data structures as linked list in order to allow 
dynamic background jobs and parsing. 

### Processing the Inputs
In order to process the `sshell` input, we separate the given arguments into tokens 
using `strtok()` and space character (` `) as the delimiter. Theses arguments are 
then stored inside the array `args[]`. We process the tokens and add them into
the argParser's args array where it can be used by `execvp` system call. We chose
to use `execvp` in order to run our commands because its ability to automatically
find programs in our `$PATH` while having a dynamic array of arguments to pass to 
the proram.We also keeps the count of (`|`) that exist inside the input and uses
it to the branch out our program into two paths. If there is no (`|`), then the 
the program will execute the built-in command or one command. Otherwise, the 
program dynamically runs multiple commands from the provided input. 

### Main Function
For built-in commands, we looked at the first argument in the array and compared
it to our various keywords such as `exit`, `cd`, and `pwd`.
The exception to this is when pipes (`|`), output
redirection (`>`), or background jobs (`&`) is used, in which case, they are
removed from `args[]` and processed independently.
We use `makeArgs` function in order to look at each command, rid of space and 
(`&`). The arguments from the command are also put inside the array. 
For pipes, we process the argParser as a linked list so the we can break down
the commands into individual processes. Based on the number of pipe symbol, we 
first fork one child process for each command. The piping commands are
executed dynamically with the for loop. Inside the loop the first child process 
will connect the output of the first command to the input of the second command. 
The second child process will connect the input of second command to output of 
the first command and the output of second command to the input of third command. 
Lastly, the third child will connect the input of the last command to the output 
of the second command. This allows us to dynamically increase the number of pipe 
commands as we demands. The function `executePipeChain` uses the `for loop` and 
`if statement` to identify the child and run the appropriate command with 
`executeParser`. The function `makePipe` is used for two commands pipelining. 
For output redirection (`>`) and appending (`>>`), we detect if the command line
uses output redirection while parsing the arguments and use booleans `redirect`
and `isOutputAppend` to mark respectively whether or not we need to redirect the
output and if we append to the file. We recorded the file location of the
redirection inside the `redirectFile` string.
In order to implement the background jobs, we first go through input and check 
if there exists the symbol (`&`). If it does, we then checks if the symbol is 
the last input of the respective command. We then use `waitpid` function with 
option flag `WNOHANG` on the respective child process. We then continuously 
monitor the return value of `waitpid` to check if the child process running in 
the background has terminated.

### Error Management
For error management, aside from library function failures, we ensures to 
initiate and free the three data structures anywhere that is necessary throughout
the program. The first thing we check from the input to our program is whether
the input is pipe or not, using `scanArgs` function. After checking for existance
of multiple commands inside the input, we check whether each command is valid 
using `makeArgs` function where the commands are broken up into token and each 
individual tokens are checked for valid command, arguments and optional flags. 
We then proceed to execute the commands. During the executions of the commands,
we carefully check if the child process is needed or not and if they are needed, 
we check if they are forked and closed successfully. In order to simplify 
potential errors, we put the execution of the command inside a function and 
implementated a loop. During the process of piping, we also ensure to close 
all the unnecessary file descriptor at every instance. After the child processes 
have finished executing the commands, we also checked the child processes exit
properly by catching errors. 

## Testing
To verify the correctness of our program, we compared the output of our
`sshell` with the reference shell as well as running all examples in the given
project proposal. The provided tester script was also used to check our program is
running correctly. During the writing of the program, everytime the new feature is added
to the progra, we utilize `printf()` function in order to make sure that each function is 
working as we intended it to work. Whenever we found error in the program, we examine 
the potential problem closely by inserting `printf()` function at each individual line 
in order to see the real cause of the error. During the process of managing errors
in the program, we also tested the program with potential inputs that can break 
the program and prevent them accordingly. 

## Limitations
The `sshell` input is limited by a few assumptions.
1. The maximum length of a command line never exceeds **512**
2. A program has a maximum of **16** non-null arguments
3. The maximum length of individual tokens doesn't exceed **32** characters

## Reference
1. https://stackoverflow.com/questions/42840950/waitpid-and-fork-execs-non-blocking-advantage-over-a-syscall
2. https://www.learn-c.org/en/Linked_lists
3. https://www.unix.com/programming/155190-pipes-connecting-3-processes-circle.html
