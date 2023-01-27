# sshell: Simple Shell

## Introduction
This project is a simple shell called **sshell** implemented using system calls
to execute inputs from the command line.

## Implementation
Intially the `sshell` input is processed into a data structure that parses the
initial input called `struct argParser`. This data structure is a list contains
information relating to the arguments.
First, we have the argument array `args[]` which contains the given values of
the input separated by spaces. We separate the given arguments using strtok
using the delimiter to be the space character(' '). We process the tokens and
add them into the argParser's args array where it can be used by `execvp` system
call. We also keeps the count of (`|`) that exist inside the input. 
We chose to use `execvp` in order to run our commands due to the ability to
automatically find programs in our `$PATH` while having a dynamic array of
arguments to pass to the program. 
For built-in commands, we looked at the first argument in the array and compared
it to our various keywords such as `exit`, `cd`, and `pwd`.
The exception to this is when pipes (`|`), output
redirection (`>`), or background jobs (`&`) is used, in which case, they are
removed from `args[]` and processed independently.
We use `makeArgs` function in order to look at each command, rid of space and (`&`). 
The arguments from the command are also put inside the array. 
For pipes, we process the argParser as a linked list so the we can break down
the commands into individual processes. Based on the number of pipe symbol, we first 
fork one child process for each command. The piping commands can be executed 
dynamically with the for loop. Inside the loop the first child process will connect
the output of the first command to the input of the second command. The second
child process will connect the input of second command to output of the first command
and the output of second command to the input of third command. Lastly, the third child
will connect the input of the last command to the output of the second command. 
This allows us to dynamically increase the number of pipe commands as we demands.
The function `executePipeChain` uses the for loop and if statement to identify 
the child and run the appropriate command with `executeParser`. The function
`makePipe` is used for two commands pipelining. 
For output redirection (`>`) and appending (`>>`), we detect if the command line
uses output redirection while parsing the arguments and use booleans `redirect`
and `isOutputAppend` to mark respectively whether or not we need to redirect the
output and if we append to the file. We recorded the file location of the
redirection inside the `redirectFile` string.


For errors, we decided to ...

## Testing
To verify the correctness of our program, we compared the output of our
**sshell** with the reference shell as well as running all examples in the given
project proposal. The provided tester script was also used to check our program is
running correctly. During the writing of the program, everytime the new feature is added
to the progra, we utilize `printf()` function in order to make sure that each function is 
working as we intended it to work. Whenever we found error in the program, we examine 
the potential problem closely by inserting `printf()` function at each individual line 
in order to see the real cause of the error. 

## Limitations
The `sshell` input is limited by a few assumptions.
1. The maximum length of a command line never exceeds **512**
2. A program has a maximum of **16** non-null arguments
3. The maximum length of individual tokens doesn't exceed **32** characters

## Reference
1. http://www.microhowto.info/howto/reap_zombie_processes_using_a_sigchld_handler.html
2. https://stackoverflow.com/questions/42840950/waitpid-and-fork-execs-non-blocking-advantage-over-a-syscall
3. https://www.learn-c.org/en/Linked_lists
4. https://www.unix.com/programming/155190-pipes-connecting-3-processes-circle.html
