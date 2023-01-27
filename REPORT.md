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
call.
We chose to use `execvp` in order to run our commands due to the ability to
automatically find programs in our `$PATH` while having a dynamic array of
arguments to pass to the program. 
For built-in commands, we looked at the first argument in the array and compared
it to our various keywords such as `exit`, `cd`, and `pwd`.
The exception to this is when pipes (`|`), output
redirection (`>`), or background jobs (`&`) is used, in which case, they are
removed from `args[]` and processed independently.
For pipes, we process the argParser as a linked list so the we can break down
the commands into individual processes.
For output redirection (`>`) and appending (`>>`), we detect if the command line
uses output redirection while parsing the arguments and use booleans `redirect`
and `isOutputAppend` to mark respectively whether or not we need to redirect the
output and if we append to the file. We recorded the file location of the
redirection inside the `redirectFile` string.

For errors, we decided to ...

## Testing
To verify the correctness of our program, we compared the output of our
**sshell** with the reference shell as well as running all examples in the given
project proposal to 

## Limitations
The `sshell` input is limited by a few assumptions.
1. The maximum length of a command line never exceeds **512**
2. A program has a maximum of **16** non-null arguments
3. The maximum length of individual tokens doesn't exceed **32** characters