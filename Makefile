#compiler
CC = gcc
#compiler flags, enable debugging
CFLAGS = -g
#remove files for a clean slate, without confirmation prompt
RM = rm -f
#build this target; all in this case
default: all
#a list of all prograns being built; here it is just myshell.c and the compelete this part of the makefile, myshell.c must be built/compiled
all: myshell.c
    $(CC) $(CFLAGS) -o myshell myshell.c
#remove the myshell executable afterwards.
clean veryclean:
    $(RM) myshell