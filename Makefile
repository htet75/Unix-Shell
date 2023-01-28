all: sshell

sshell: sshell.c
	gcc -Wall -Wextra -Werror -O2 -o sshell sshell.c

clean:
	rm -f sshell
