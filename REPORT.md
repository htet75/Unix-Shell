# sshell: Simple Shell

## Introduction
This project is a simple shell called **sshell** implemented using system calls
to execute inputs from the command line.
## Implementation
Intially the `sshell` input is processed into a data structure that parses the
initial input called `struct argParser`. This data structure contains
information relating to the arguments.
First, we have the argument array `args[]` which contains the given values of
the input separated by spaces. The exception to this is when pipes or output
redirection is used, in which case, they are removed from `args[]` and processed
individually.

## Limitations
The `sshell` input is limited by a few assumptions.
1. The maximum length of a command line never exceeds **512**
2. A program has a maximum of **16** non-null arguments
3. The maximum length of individual tokens doesn't exceed **32** characters