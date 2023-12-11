# BASLOAD ROM version

## Introduction

The purpose of BASLOAD is to make programming in BASIC on the Commander X16 more convenient.

This is primarily done by using named labels instead of line numbers, and by supporting
long variable names.

The source code is stored as a text file on the SD card, and then converted to a runnable
BASIC program by the tool.


## Source code formatting

### No line numbers

Source code written for BASLOAD may not contain line numbers. Instead, named labels are decleared
as target for BASIC commands that need a line number, for instance GOTO and GOSUB.


### Same BASIC commands

BASLOAD uses the standard BASIC commands of the built-in BASIC.


### Whitespace

The following characters are recognized as whitespace in the source code:

- Blank space (PETSCII 32)
- Shift + blank space (PETSCII 160)
- Tab (PETSCII 9)


## Identifiers

### General

Identifiers refer to BASIC commands, labels, and variables.

All identifiers must begin with a letter, any of A to Z.

The subsequent characters may be a letter (A to Z), a digit (0 to 9) or a decimal point.

Identifiers are not case-sensitive.

An identifier may be at most 64 characters long.

Unless two adjacent identifiers are not otherwise separated by a character outside the
group of characters allowed in identifier names, the identifiers must be separated
by whitespace.

Example:

```
PRINTME     ; refers to a variable or label named PRINTME
PRINT ME    ; will PRINT the value of ME
```

### Labels

A label must be a valid identifier name. 

A label declaration must occur at the beginning of a line, but there may be whitespace before it. The
declaration of the label ends with a colon.

You use a label in the source code by typing its name without the colon.

Example:

```
LOOP:
    PRINT "HELLO, WORLD!"
    GOTO LOOP
```

A label may not be exactly the same as any BASIC command or reserved word, or exactly
the same as a previously decleared variable.


### Variables

A variable must be a valid identifier name.

Variables are automatically decleared when found in the source code.

A variable may not be exactly the same as any BASIC command or reserved word, or
exacly the same as a previously decleared label.

Example:

```
HELLO.WORLD.GREETING$ = "Hello, world!"
PRINT HELLO.WORLD.GREETING
```

## BASLOAD options

There are some options to control the output from BASLOAD.

Options must be at the beginning of a line, but there may be
whitespace before it.

All options start with a hash sign (#) followed by the name of the option.

Some options require a parameter. The option name and the parameter must be
separated by whitespace.

If the parameter is a decimal number, you type it in as is.

If the parameter is a string it may optionally be enclosed in double quotes. If so, it
may include whitespace characters.

The following options are supported:

- \## An alternative comment never outputted to the resulting code
- #REM <0|1>, turns off (0) or on (1) output of REM statements. Default is off.
- #INCLUDE <filename>, includes the content of another source file 
- #AUTONUM <1..100>, sets the number of steps the line number is advanced for each line in the output
- #CONTROLCODES <0|1>, turns off (0) or on (1) support for named PETSCII control characters, for instance {WHITE} or {CLEAR}

## Running BASLOAD

TODO