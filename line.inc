;******************************************************************************
;Function name: line_pass1
;Purpose......: Load pass 1; looks for label defintions and stores them in
;               the symbol table
;Input........: Nothing
;Output.......: A = 0 if OK
;Errors.......: A = 1 if error occured
.proc line_pass1
.endproc

;******************************************************************************
;Function name: line_pass2
;Purpose......: Load pass 2; tokenizes the source code and replaces label
;               references with the corresponding line numbers stored in
;               the symbol table during pass 1
;Input........: Nothing
;Output.......: A = 0 if OK
;Errors.......: A = 1 if error occured
.proc line_pass2
    ; Init
    stz non_whitespace_count
    stz index
    stz outindex
    stz element

loop:
    ldx index
    lda file_buf,x
    ldx element
    jmp (jump_table,x)

default:
    ; White space
    cmp #32
    beq :+
    cmp #160
    beq :+
    cmp #9
    bne :++
:   jmp next
:   inc non_whitespace_count

    ; String
    cmp #34
    bne :+
    ldx #LINE_STRING
    stx element
    jmp copy

    ; Convert upper/lower case
:   and #%11011111

    ; Option
    cmp #35
    bne :+
    ldx non_whitespace_count
    bne :+
    lda #LINE_OPTION_1
    sta element
    ldx index
    stx element_start
    jmp next

    ; Hex number
:   cmp #36
    bne :+
    ldx #LINE_HEXNUM
    stx element
    jmp copy

    ; Binary number
:   cmp #37
    bne :+
    ldx #LINE_BINNUM
    stx element
    jmp copy

    ; Decimal number starting with decimal point
:   cmp #46
    bne :+
    ldx #LINE_DECNUM
    stx element
    ldx #1
    stx dp_count
    jmp copy

    ; Other decimal number
:   cmp #48
    bcc :++
    cmp #58
    bcs :+
    ldx #LINE_DECNUM
    stx element
    jmp copy

    ; Symbol
:   cmp #65
    bcc :++
    cmp #91
    bcs :+
    ldx #LINE_SYMBOL
    stx element
    ldx index
    stx element_start
    ldx non_whitespace_count
    stz is_first
    cpx #1
    bne :+
    stx is_first
:   jmp next

    ; Other
:   jmp copy

remark:
    ldx remarks_on
    beq :+
    jmp copy
:   jmp next

string:
    cmp #34
    bne :+
    ldx #LINE_DEFAULT_ELEMENT
    stx element
:   jmp copy

option1:

option2:

option3:

option4:

hexnum:
    ; 0..9?
    cmp #48
    bcc :+
    cmp #58
    bcs :+
    jmp copy

:   ; A..Z?
    cmp #65
    bcc :+
    cmp #91
    bcs :+
    jmp copy

:   stz element
    jmp loop

binnum:
    ; 0..1?
    cmp #48
    beq :+
    cmp #49
    beq :+

    stz element
    jmp loop

:   jmp copy

decnum:
    ; Decimal point?
    cmp #46
    bne :++
    inc dp_count
    ldx dp_count
    cpx #2
    bcs :+
    jmp copy

:   stz element
    jmp loop

    ; 0..9?
:   cmp #48
    bcc :+
    cmp #58
    bcs :+
    jmp copy
:   stz element
    jmp loop

symbol:
    cmp #46
    beq symbol_cont

    cmp #48
    bcc symbol_stop
    cmp #58
    bcc symbol_cont

    cmp #58
    beq symbol_label

    cmp #65
    bcc symbol_stop
    cmp #91
    bcs symbol_stop

symbol_cont:
    jmp next

symbol_label:
    ldx is_first
    beq symbol_stop
    ldx element_start
    stx $6100
    ldx index
    stx $6101

symbol_stop:
    ldx element_start
    ldx index
    stz element
    jmp next    

copy:
    ldx outindex
    sta outbuf,x    
    inc outindex

next:
    ldx index
    lda file_buf,x
    beq eof
    inc index
    jmp loop

eof:
    rts

jump_table:
    .word default, remark, string, option1, option2, option3, option4, hexnum, binnum, decnum, symbol

    .segment "VARS"
        non_whitespace_count: .res 1
        index: .res 1
        element: .res 1
        element_start: .res 1
        dp_count: .res 1
        outindex: .res 1
        remarks_on: .res 1
        is_first: .res 1
    .CODE
.endproc

LINE_DEFAULT_ELEMENT = 0
LINE_REMARK = 2
LINE_STRING = 4
LINE_OPTION_1 = 6
LINE_OPTION_2 = 8
LINE_OPTION_3 = 10
LINE_OPTION_4 = 12
LINE_HEXNUM = 14
LINE_BINNUM = 16
LINE_DECNUM = 18
LINE_SYMBOL = 20

outbuf = $6000

line_srclin = 0
line_init = 0

text: .byt "   hello: print ",0

.segment "VARS"
    line_dstlin: .res 1
.CODE