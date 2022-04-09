## [M1]
#  Explaint the following here
#  To get the directory where the Makefile resides
#  The command can be broken down to
#  THIS_FILE := $(realpath $(lastword $(MAKEFILE_LIST)))
#  THIS_FILE_DIR := $(shell dirname $(THIS_FILE))

CUR_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

## [M2]
#  Explaint the following here
#  define the C compiler to use [CC = gcc]
#  CFLAGS is a variable used to add arguments 
#  gcc -g generates debug information to be used by GDB debugger.
#  -O2 flag to set optimization level for code size and execution time 
#  Used -Werror to tell GCC to treat all the warnings as errors.
#  used -Wall to enable all compiler's warning messages.
#  LDFLAGS is a variable used to provide list of link/load directives
#  LDFLAGS are passed to the linker
#  ...
CC = gcc
CFLAGS += -g -O2 -Werror -Wall
LDFLAGS +=

## [M3] -- REMOVED
#  ...

## [M5]
#  Explaint the following here
#  all files with the .c extension will be stored in a variable SRCS_FILES.
#  ...
SRC_FILES = $(wildcard *.c)

## [M6]
#  Explaint the following here
#  Remove the .c extension of SRC_FILES. These files without .c extension will be generated as output after compilation
EXE_FILES = $(SRC_FILES:.c=)

## [M7]
#  Explaint the following here
#  Make provides the .PHONY target syntax to mark a target as not pointing to a file and all is one of the phony rule.
#  a phony rule that compiles all our SRC_FILES to EXE_FILES.
#  ...
all: $(EXE_FILES)

## [M8] -- UPDATED
#  Explaint the following here
#  %:%.c resembles target:prerequisite.Here target will not have any extension i.e. EXE_FILES whereas prerequisite file will have .c extension 
# '$<' automatic variable, specifies the first prerequisite which is the .c file.
# '$@' automatic variable, specifies the file name of the target of the rule.
#  $(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< will evaluate to gcc -g -O2 -Werror -Wall -o target prerequisite
#  ...
%:%.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< 

## [M9]
#  Explaint the following here
#  Make rule for cleaning all the object files/executables from the directory
#  ...
clean:
	rm -f $(EXE_FILES)

## [M10]
#  Explaint the following here
#  Phony (Psuedo) Targets - target that does not have a real, corresponding file (file that we want to create).
#  a phony rule that removes/clean all the object files/executables from the directory.
#  ...
.PHONY: all clean
