# BASLOAD ROM version

This is work in progress, and is not yet functional.

## Planned features

- Source code without line numbers: Labels are declared as targets for GOTO and other commands that use line numbers.

- Long variable names: The BASIC v2 has two significant characters in variable names. This is expanded to 27.

- Labels:
    - A label must be declared at the beginning of the line.
    - There may, however, be white space before the label starts.
    - A label may not be exactly the same as a BASIC command/token.
    - The first character must be within A..Z.
    - Subsequent characters must be within A..Z, 0..9 or a dot (.).
    - A label declaration ends with a colon (:).
    - Examples:
        - MY.LABEL1: is ok
        - PRINT.ME: is a valid label
        - PRINT: is not a valid label

- Variables:
    - Variables are automatically declared the first time they are used in the source code.
    - A variable may not be exactly the same as a BASIC command/token.
    - The first character must be within A..Z.
    - Subsequent characters must be within A..Z, 0..9 or a dot (.).
    - Examples:
        - PRINT is not a valid variable
        - PRINTME is ok
    
- White space:
    - Some white space is needed to part identifiers from each other.
    - Any char that is not allowed in labels or variables (A..Z, 0..9, or dot) will be enough
    - If one identifier ends with and the next identifier begins with such a char, some white space is needed
    - Examples:
        - PRINT"HELLO" is OK as the double quotes tells us that the PRINT statement ended
        - PRINTMESSAGE$ will not PRINT MESSAGE$ unless there is white space between the identifiers