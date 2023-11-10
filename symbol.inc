;BSD 2-Clause License
;
;Copyright (c) 2021-2023, Stefan Jakobsson
;All rights reserved.

;Redistribution and use in source and binary forms, with or without
;modification, are permitted provided that the following conditions are met:
;
;1. Redistributions of source code must retain the above copyright notice, this
;   list of conditions and the following disclaimer.
;
;2. Redistributions in binary form must reproduce the above copyright notice,
;   this list of conditions and the following disclaimer in the documentation
;   and/or other materials provided with the distribution.
;
;THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
;AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
;IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
;DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
;FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
;DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
;SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
;CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
;OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
;OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

; Symbol table layout
; -------------------
; In this code, symbols refer both to label and variable names. The symbol
; table data is stored in RAM banks SYMBOL_FIRST_BANK to
; SYMBOL_LAST_BANK.
; 
; A checksum is calculated for each symbol name, simply the LSB of
; the sum of its char values. The checksum is used to speed up
; symbol search.
;
; The symbols are stored in 256 separate "buckets", one for each
; possible checksum value. Each bucket is a linked list of 
; symbols with the same checksum.
;
; A reference to the head of each bucket is stored
; in the arrays symbol_buckets_bank and symbol_buckets_offset. These
; arrays are placed in RAM bank BASLOAD_RAM1.
; 
; An entry in the table consists of 32 bytes as described below:
; 
; Offset Size Description
; ------------------------------------------------
; 00     26   Symbol name
; 26     01   Symbol name length
; 27     01   Next symbol, bank (0=null)
; 28     01   Next symbol, offset
; 29     01   Symbol type
; 30     02   Value
;
; You can convert from offset to memory address as follows: 
; address = offset * 32 + $A000
;
; The table may hold a maximum of 2,048 symbols. X16 Basic
; supports a maximum of 950 different variable names that
; may be freely used.

; Banked RAM usage
SYMBOL_FIRST_BANK = 2
SYMBOL_LAST_BANK = 9

; Symbol table entry fields
SYMBOL_NAME = 0
SYMBOL_LEN = 26
SYMBOL_NEXT_BANK = 27
SYMBOL_NEXT_OFFSET = 28
SYMBOL_TYPE = 29
SYMBOL_VALUE = 30

; Symbol entry types
SYMBOLTYPE_LABEL = 0
SYMBOLTYPE_FLOAT = 1
SYMBOLTYPE_INT = 2
SYMBOLTYPE_STRING = 3

.ZEROPAGE
    symbol_pointer: .res 2

.segment "VARS"
    symbol_next_var: .res 2
    symbol_next_bank: .res 1
    symbol_next_offset: .res 1

.segment "RAM1"
    symbol_buckets_bank: .res $100
    symbol_buckets_offset: .res $100
.CODE

; Exports needed only for unit testing
.export symbol_init, symbol_add, symbol_find, symbol_buckets_bank, symbol_buckets_offset, file_buf, line_dstlin, symbol_next_bank, symbol_next_offset, symbol_pointer

;******************************************************************************
;Function name: symbol_init
;Purpose......: Initializes symbol functions and data
;Input........: Nothing
;Output.......: Nothing
;Error........: None
.proc symbol_init
    ; Set all buckets to null
    lda #BASLOAD_RAM1
    sta RAM_SEL

    ldx #0
    lda #0
:   sta symbol_buckets_bank,x
    inx
    cpx #0
    bne :-

    ; Set next symbol table entry location in banked RAM
    lda #SYMBOL_FIRST_BANK
    sta symbol_next_bank
    stz symbol_next_offset

    ; Set next available variable name to "A"
    lda #'a'
    sta symbol_next_var
    lda #0
    sta symbol_next_var+1
    
    rts
.endproc

;******************************************************************************
;Function name: symbol_add
;Purpose......: Adds an element to the symbol table
;Input........: X   Index in file_buf where symbol starts
;               Y   Index in file_buf where symbol ends
;               A   Symbol type
;                   0: Label
;                   1: Variable (float)
;                   2: Variable (int)
;                   3: Variable (string)
;               C   1: Skip duplicate symbol check
;Output.......: X/Y Symbol value (low/high byte)
;               A   0: OK
;Error........: A   1: Duplicate symbol
;                   2: Symbol table full
;                   3: No available variable name
.proc symbol_add
    ; Store input
    stx index1
    sty index2
    sta type

    ; Check if duplicate
    bcs :+  ; Skip duplicate label check if C=1
    jsr symbol_find
    bcs :+ ; Symbol not found, continue
    lda #1
    rts 

    ; Check if symbol table is full
:   lda symbol_next_bank
    cmp #SYMBOL_LAST_BANK+1
    bcc :+
    lda #2
    rts

    ; Check variable name availability
:   lda type
    cmp #SYMBOLTYPE_LABEL
    bne :+
    lda symbol_next_var
    cmp #'z'+1
    bcc :+
    lda #3
    rts

    ; Calculate symbol name checksum
:   stz cs
    ldx index1
:   clc
    lda file_buf,x
    adc cs
    sta cs
    cpx index2
    beq :+
    inx
    bra :-

:   ; Calculate len
    sec
    lda index2
    sbc index1
    inc
    sta len

    ; Check if bucket is empty
    lda #BASLOAD_RAM1
    sta RAM_SEL
    ldx cs
    lda symbol_buckets_bank,x
    bne bucket_not_empty

bucket_empty:
    lda symbol_next_bank
    sta symbol_buckets_bank,x
    lda symbol_next_offset
    sta symbol_buckets_offset,x
    bra set_values

bucket_not_empty:
    pha
    lda symbol_buckets_offset,x
    plx ; find_tail expects bank in .X

find_tail:
    ; Select RAN bank where item is stored
    stx RAM_SEL
    
    ; Get address = offset * 32 + $a000
    sta symbol_pointer
    stz symbol_pointer+1
    .repeat 5
        asl symbol_pointer
        rol symbol_pointer+1
    .endrep
    clc
    lda symbol_pointer+1
    adc #$a0
    sta symbol_pointer+1

    ; Get next bank field, 0 => we're at end of the linked list
    ldy #SYMBOL_NEXT_BANK
    lda (symbol_pointer),y
    beq tail_found
    
    ; There're more items in the list, prepare to look at next item
    tax ; Next bank stored in X
    iny ; Index for next offset
    lda (symbol_pointer),y
    bra find_tail

tail_found:
    ; Store link to the new item we're inserting in the current tail item
    lda symbol_next_bank
    sta (symbol_pointer),y
    iny
    lda symbol_next_offset
    sta (symbol_pointer),y

set_values:
    ; Select RAM bank where the new item is stored
    lda symbol_next_bank
    sta RAM_SEL

    ; Set address to new item = offset * 32 + $a000
    lda symbol_next_offset
    sta symbol_pointer
    stz symbol_pointer+1
    .repeat 5
        asl symbol_pointer
        rol symbol_pointer+1
    .endrep
    clc
    lda symbol_pointer+1
    adc #$a0
    sta symbol_pointer+1

    ; Copy symbol name
    ldx index1
    ldy #SYMBOL_NAME
:   lda file_buf,x
    sta (symbol_pointer),y
    cpx index2
    beq :+
    inx
    iny
    bra :-

:   ; Set symbol name len
    ldy #SYMBOL_LEN
    lda len
    sta (symbol_pointer),y

    ; Set next bank to 0 = NULL
    ldy #SYMBOL_NEXT_BANK
    lda #0
    sta (symbol_pointer),y

    ; Set symbol type
    ldy #SYMBOL_TYPE
    lda type
    sta (symbol_pointer),y

    ; Check if symbol type is Label or Variable
    ldy #SYMBOL_VALUE
    cmp #0
    bne variable

    ; New item is a label: Set value to current line number
    lda line_dstlin
    sta (symbol_pointer),y
    tax
    iny
    lda line_dstlin+1
    sta (symbol_pointer),y
    tay
    bra exit

variable:
    ; New item is a variable: Set value to next available variable name
    lda symbol_next_var
    sta (symbol_pointer),y
    pha
    iny
    lda symbol_next_var+1
    sta (symbol_pointer),y
    tay

inc_var:
    ; Advance second character of the next available variable name
    inc symbol_next_var+1
    lda symbol_next_var+1
    
    ; If 1 => The current variable name is one character, next variable ends with '0'
    cmp #1
    beq :+
    
    ; Current variable name ends with a '9' => The next variable will end with 'A'
    cmp #'9'+1
    beq :++
    
    ; The current variable name ends with a 'Z' => The next variable is a single char
    cmp #'z'+1
    bne chk_var

    inc symbol_next_var
    stz symbol_next_var+1
    bra chk_var

:   lda #'0'
    sta symbol_next_var+1
    bra chk_var

:   lda #'a'
    sta symbol_next_var+1

chk_var:
    ; Check if the next variable name is reserved
    ldx #0

chk_var_loop:
    ; Compare first char
    lda symbol_reserved_vars,x
    inx
    cmp symbol_next_var
    bne :+
    
    ; Compare second char
    lda symbol_reserved_vars,x
    inx

    ; There's a match in the reserved names table => try again
    cmp symbol_next_var+1
    beq inc_var

    ; Are we done?
    cpx #symbol_reserved_vars_end-symbol_reserved_vars
    bne chk_var_loop
    bra :++

:   ; Are we done?
    inx
    cpx #symbol_reserved_vars_end-symbol_reserved_vars
    bne chk_var_loop

:   ; Pull symbol low value from stack
    plx

exit:
    ; Select next offset
    inc symbol_next_offset
    bne :+
    
    ; Increment bank
    inc symbol_next_bank

:   lda #0
    rts

.segment "VARS"
    index1: .res 1
    index2: .res 2
    type: .res 1
    cs: .res 1
    len: .res 1
.CODE
.endproc

;******************************************************************************
;Function name: symbol_find
;Purpose......: Searches symbol table for a given symbol name
;Input........: X   Index in file_buf where symbol starts
;               Y   Index in file_buf where symbol ends
;Output.......: X,Y Value (low, high byte)
;               A   Symbol type
;                   0: Label
;                   1: Variable (float)
;                   2: Variable (int)
;                   3: Variable (string)
;Error........: C=1 if label not found
.proc symbol_find
    ; Store input params
    stx index1
    sty index2

    ; Calculate symbol name length
    sec
    lda index2
    sbc index1
    ina
    sta len
    
    ; Calculate symbol name checksum
    stz cs
    ldx index1
:   clc
    lda file_buf,x
    adc cs
    sta cs
    cpx index2
    beq :+
    inx
    bra :-

    ; Check if bucket head is NULL
:   lda #BASLOAD_RAM1               ; Select RAM bank 1
    sta RAM_SEL
    
    ldx cs
    lda symbol_buckets_bank,x
    beq not_found                   ; Bucket head was null => symbol doesn't exist

    ; OK, let's look what we find in the symbol table...
    tay                             ; Store bank in Y temporarily

    lda symbol_buckets_offset,x     ; Get offset
    sta symbol_pointer
    stz symbol_pointer+1

loop:
    .repeat 5                       ; Calculate offset x 32 + $a000 to get bucket item address
        asl symbol_pointer
        rol symbol_pointer+1
    .endrep
    clc
    lda symbol_pointer+1
    adc #$a0
    sta symbol_pointer+1
    
    sty RAM_SEL                     ; Set RAM bank where item is stored

    ldy #SYMBOL_LEN                 ; Compare lengths
    lda (symbol_pointer),y
    cmp len
    bne next

    ldy #SYMBOL_NAME                ; Compare names
    ldx index1
:   lda (symbol_pointer),y
    cmp file_buf,x
    bne next
    cpx index2
    beq found
    inx
    iny
    bra :-

next:                               ; It wasn't a match, look at next item in the bucket
    ldy #SYMBOL_NEXT_BANK
    lda (symbol_pointer),y
    beq not_found                   ; Next bank = 0 => No more items in this bucket
    
    pha                             ; Store bank on stack
    ldy #SYMBOL_NEXT_OFFSET
    lda (symbol_pointer),y
    sta symbol_pointer
    stz symbol_pointer+1
    ply                             ; Get bank
    bra loop

found:
    ldy #SYMBOL_TYPE
    lda (symbol_pointer),y
    pha
    ldy #SYMBOL_VALUE
    lda (symbol_pointer),y
    tax
    ldy #SYMBOL_VALUE+1
    lda (symbol_pointer),y
    tay
    pla
    clc
    rts

not_found:
    sec
    rts

.segment "VARS"
    index1: .res 1
    index2: .res 2
    cs: .res 1
    len: .res 1
.CODE

.endproc

symbol_reserved_vars:
    .byt "da"
    .byt "if"
    .byt "fn"
    .byt "go"
    .byt "mx"
    .byt "my"
    .byt "mb"
    .byt "on"
    .byt "or"
    .byt "st"
    .byt "ti"
    .byt "to"
symbol_reserved_vars_end:
