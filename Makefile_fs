# Compiler
CC = gcc
# Compiler flags, enable debugging
CFLAGS = -g
# Remove files for a clean slate, without confirmation prompt
RM = rm -f
# Build this target; all in this case
default: all
# A list of all programs being built; here it is just fs.c and the complete this part of the Makefile, fs.c must be built/compiled
all: fs.c
    $(CC) $(CFLAGS) -o fs fs.c
# Remove the myshell executable afterwards
clean veryclean:
    $(RM) fs