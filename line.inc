; Exports needed for unit testing
.export line_init, line_pass1, line_pass2

; Line parsing states
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

; Variables
.ZEROPAGE
    line_meta: .res 2
    line_code: .res 2

.segment "VARS"
    line_srclin: .res 3
    line_dstlin: .res 2
.CODE

;******************************************************************************
;Function name: line_init
;Purpose......: Initialized the line parsing functions; to be called once
;               before pass1 and once before pass 2
;Input........: Nothing
;Output.......: Nothing
;Errors.......: None
.proc line_init
    ; Clear source line number
    stz line_srclin
    stz line_srclin+1
    stz line_srclin+2

    ; Clear destination line number
    stz line_dstlin
    stz line_dstlin+1

    ; Setup pointers to BASIC memory, starting at $0801
    lda #$01
    sta line_meta
    lda #$05
    sta line_code
    lda #$08
    sta line_meta+1
    sta line_code+1

    rts
.endproc

;******************************************************************************
;Function name: line_pass1
;Purpose......: Pass 1; looks for label defintions and stores them in
;               the symbol table
;Input........: Nothing
;Output.......: A = 0 if OK
;Errors.......: A = 1 if error occured
.proc line_pass1
    ; Init
    stz non_whitespace_count
    stz index_src
    stz index_dst
    stz state

    ; Increment source line
    inc line_srclin
    bne :+
    inc line_srclin+1
    bne :+
    inc line_srclin+2

    ; Increment destination line
:   inc line_dstlin
    bne loop
    inc line_dstlin+1

loop:
    ; Get next char
    ldx index_src
    lda file_buf,x
    bne switch_state

eol:
    ; Rewind destination line number if the line didn't output anything to BASIC memory
    lda index_dst
    bne :++

    lda line_dstlin
    bne :+
    dec line_dstlin+1
:   dec line_dstlin

    ; Set response value, 0 = OK
:   lda #0
    rts

switch_state:
    ldy state
    cpy #LINE_SYMBOL
    beq symbol

default:
    ; Whitespace - Ignored outside strings and REM statements
    cmp #32 ; Blank space
    beq :+
    cmp #160 ; Shift+blank space
    beq :+
    cmp #9 ; Tab
    bne :++
:   jmp next
:   inc non_whitespace_count

    ; Convert all alphabetic chars to the same case
    cmp #$40
    bcc :+
    and #%11011111
    
    ; Symbol start?
:   cmp #'a'
    bcc fake_copy
    cmp #'z'+1
    bcs fake_copy

    ; Yes it's a symbol
    stx symbol_start
    lda #LINE_SYMBOL
    sta state
    jmp next

symbol:
    ; Convert all alphabetic chars to the same case
    cmp #$40
    bcc :+
    and #%11011111

    ; Alphabetic chars are allowed in symbol names, continue if found
:   cmp #'a'
    bcc :+
    cmp #'z'+1
    bcc next

    ; Points are allowed in a symbol name, continue if found
:   cmp #'.'
    beq next

    ; A symbol ended by a colon is a possible label declaration - needs further checking
    cmp #':'
    beq label

    ; Decimal numbers are allowed in symbol names, continue if found
    cmp #'0'
    bcc symbol_end
    cmp #'9'+1
    bcs symbol_end

label:
    lda index_dst
    bne symbol_end

    ldx symbol_start
    ldy index_src
    dey
    jsr token_get
    bcc symbol_end

    ldx symbol_start
    ldy index_src
    lda #0
    dey
    jsr symbol_add
    stz state
    jmp next

symbol_end:
    inc index_dst
    stz state
    jmp loop

fake_copy:
    ; In pass 2 this data will be copied to BASIC memory
    ; We need to keep track of it, in order to tell that this will output data to BASIC memory
    inc index_dst

next:
    inc index_src
    jmp loop

.segment "VARS"
    non_whitespace_count: .res 1
    index_src: .res 1
    index_dst: .res 1
    state: .res 1
    symbol_start: .res 1
.CODE
.endproc

;******************************************************************************
;Function name: line_pass2
;Purpose......: Load pass 2; tokenizes the source code and replaces long
;               variable names with short names supported by BASIC v 2
;Input........: Nothing
;Output.......: A = 0 if OK
;Errors.......: A = 1 if error occured
.proc line_pass2
    ; Init
    stz non_whitespace_count
    stz index_src
    stz index_dst
    stz state
    
    ; Option: Remarks on/off, default is on
    lda #1
    sta remarks_on

    ; Increment source line
    inc line_srclin
    bne :+
    inc line_srclin+1
    bne :+
    inc line_srclin+2

    ; Increment destination line
:   inc line_dstlin
    bne loop
    inc line_dstlin+1

loop:
    ; Get next char from file buffer
    ldx index_src
    lda file_buf,x
    bne switch_state

eol:
    ; Rewind destination line number if nothing was outputted to BASIC memory
    ldy index_dst
    bne :++
    
    lda line_dstlin
    bne :+
    dec line_dstlin+1
:   dec line_dstlin
    bra exit

    ; Update BASIC memory: Set end of line marker
:   lda #0
    sta (line_code),y
    inc index_dst

    ; Update BASIC memory: Set pointer to next line
    clc
    lda line_code
    adc index_dst
    sta line_code
    lda line_code+1
    adc #0
    sta line_code+1

    ldy #1
    sta (line_meta),y
    dey
    lda line_code
    sta (line_meta),y

    ; Update BASIC memory: Set line number
    ldy #2
    lda line_dstlin
    sta (line_meta),y
    iny
    lda line_dstlin+1
    sta (line_meta),y
    
    ; Point line_meta and line_code to next line
    clc
    lda line_code
    sta line_meta
    adc #4
    sta line_code

    lda line_code+1
    sta line_meta+1
    adc #0
    sta line_code+1

    ; Update BASIC memory: Clear start of next line
    ldy #0
    lda #0
    sta (line_meta),y
    iny
    sta (line_meta),y

exit:
    ; Exit with status 0 = OK
    lda #0
    rts

switch_state:
    ; Jump to state handling code
    ldx state
    jmp (jump_table,x)

default:
    ; Whitespace - Ignored outside strings and REM statements
    cmp #32 ; Blank space
    beq :+
    cmp #160 ; Shift+blank space
    beq :+
    cmp #9 ; Tab
    bne :++
:   jmp next
:   inc non_whitespace_count

    ; String
    cmp #34 ; Double quote
    bne :+
    ldx #LINE_STRING
    stx state
    jmp copy

    ; Convert all alphabetic chars to the same case
:   cmp #$40
    bcc :+
    and #%11011111

    ; Option - Starts with the # sign, must be at beginning of line
:   cmp #35 ; Hash sign
    bne :+
    ldx non_whitespace_count ; Check if it's the first non-whitespace char on this line
    bne :+
    lda #LINE_OPTION_1
    sta state
    ldx index_src
    stx symbol_start
    jmp next

    ; Hexadecimal number - Starts with a $ sign
:   cmp #'$'
    bne :+
    ldx #LINE_HEXNUM
    stx state
    jmp copy

    ; Binary number - Starts with a % sign
:   cmp #'%'
    bne :+
    ldx #LINE_BINNUM
    stx state
    jmp copy

    ; Decimal number - Alternative 1, starting with a decimal point
:   cmp #'.'
    bne :+
    ldx #LINE_DECNUM
    stx state
    ldx #1
    stx dp_count
    jmp copy

    ; Decimal number - Alternative 2, starting with a number 0..9
:   cmp #'0'
    bcc :+
    cmp #'9'+1
    bcs :+
    ldx #LINE_DECNUM
    stx state
    jmp copy

    ; Symbol - A BASIC command, a label or a variable, starting with chars A..Z
:   cmp #'a'
    bcc :+
    cmp #'z'+1
    bcs :+
    ldx #LINE_SYMBOL
    stx state
    ldx index_src
    stx symbol_start
    jmp next

    ; BASIC operators, eg. +, -, *, / - Tokenized as commands
:   ldx index_src
    ldy index_src
    jsr token_get
    bcs :+
    
    ldy index_dst
    sta (line_code),y
    inc index_dst
    jmp next
    
    ; Other input
:   ldx index_src
    lda file_buf,x
    jmp copy

remark:
    ldx remarks_on
    beq :+
    jmp copy
:   jmp next

string:
    cmp #34 ; Double quote
    bne :+
    ldx #LINE_DEFAULT_ELEMENT
    stx state
:   jmp copy

option1:
    ; TODO: Not yet implemented
    
option2:
    ; TODO: Not yet implemented

option3:
    ; TODO: Not yet implemented

option4:
    ; TODO: Not yet implemented
    jmp next

hexnum:
    ; 0..9?
    cmp #'0'
    bcc :+
    cmp #'9'+1
    bcs :+
    jmp copy

:   ; A..F?
    cmp #'a'
    bcc :+
    cmp #'f'+1
    bcs :+
    jmp copy

:   ; The hex num ended at the previous char: clear state, and jump to beginning of loop so that the default handler can process this char
    stz state
    jmp loop

binnum:
    ; 0..1?
    cmp #'0'
    beq :+
    cmp #'1'
    beq :+

    ; The binary num ended at the previous char: clear state, and jump to beginning of loop so that the default handler can process this char
    stz state
    jmp loop

:   jmp copy

decnum:
    ; Decimal point?
    cmp #'.'
    bne :++

    inc dp_count
    ldx dp_count
    cpx #2  
    bcc :+

    ; There were more than 1 decimal points: clear state and jump to beginning of loop so that the default handler can process this char
    stz state
    jmp loop

:   jmp copy

    ; 0..9?
:   cmp #'0'
    bcc :+
    cmp #'9'+1
    bcs :+
    jmp copy

:   ; The decimal number ended at the previous char: clear state and jump to beginning of loop so that the default handler can process this char
    stz state
    jmp loop

symbol:
    ; Convert letters to the same case, leave other chars alone
    cmp #$40
    bcc :+
    and #%11011111

    ; Alphabetic chars are allowed in symbol names, continue if found
:   cmp #'a'
    bcc :+
    cmp #'z'+1
    bcc symbol_cont

    ; Points are allowed in a symbol name, continue if found
:   cmp #'.'
    beq symbol_cont

    ; A symbol ended by a colon is a possible label declaration - needs further checking
    cmp #':'
    beq label_declaration

    ; Decimal numbers are allowed in symbol names, continue if found
    cmp #'0'
    bcc symbol_other
    cmp #'9'+1
    bcs symbol_other

symbol_cont:
    ; Continue, get next char
    jmp next

label_declaration:
    ; Check if the symbol is at the start of the line
    ldx index_dst
    bne symbol_other ; It wasn't, so it can't be a label declaration

    ; Check if BASIC command/token
    ldx symbol_start
    ldy index_src
    dey ; Don't include the colon
    jsr token_get
    bcs :+ ; C=1, token not found
    
    ; It was a BASIC command/token, store to BASIC memory, clear state and jump to loop so that the default handler can process the colon
    ldy index_dst
    sta (line_code),y
    inc index_dst
    stz state
    jmp loop

:   ; It was a label declaration, let's ignore it in pass 2! Jump to next to ignore the colon as well
    stz state
    jmp next

symbol_other:
    ; Check if it is a BASIC command/token (including the current char)
    ldx symbol_start
    ldy index_src
    jsr token_get
    bcs :+

    ; It was a token, insert it, and then jump to next
    ldy index_dst
    sta (line_code),y
    inc index_dst
    stz state
    jmp next

:   ; Check if it is a BASIC command/token (not including the current char)
    ldx symbol_start
    ldy index_src
    dey ; Step back to ignore the current char
    jsr token_get
    bcs :+

    ; It was a token, insert it, and jump to loop so that the default handler can process the current char
    ldy index_dst
    sta (line_code),y
    inc index_dst
    stz state
    jmp loop

    ; Check if it is a user-defined symbol (not including the current char), previously decleared
:   ldx symbol_start
    ldy index_src
    dey ; Step back to ignore the current char
    jsr symbol_find
    bcs newvar
    
    cmp #1
    beq insvar

    ; It's a label, insert its line number
    jsr util_bin_to_bcd
    jsr util_bcd_to_str
    stx TEMP1
    sty TEMP1+1
    ldy #0
:   lda (TEMP1),y
    beq :+
    phy
    ldy index_dst
    sta (line_code),y
    inc index_dst
    ply
    iny
    bra :-

:   stz state
    jmp next

newvar:   
    ; The symbol hasn't been decleared. Let's declear it as a new variable
    ldx symbol_start
    ldy index_src
    dey ; Step back to ignore the current char
    lda #1 ; variable
    jsr symbol_add

insvar:
   ; Store variable value to BASIC memory
    phy
    txa
    ldy index_dst
    sta (line_code),y
    iny
    pla
    beq :+
    sta (line_code),y
    inc index_dst
:   inc index_dst

    ; Clear state
    stz state

    ; If the current char is $ or %, let's copy it to BASIC memory, otherwise let the default handler process it
    ; A trailing $ or % indicates the type of variable
    ldx index_src
    lda file_buf,x
    cmp #'$'
    beq copy
    cmp #'%'
    beq copy
    jmp loop

copy:
    ; Copy current char to BASIC memory
    ldy index_dst
    sta (line_code),y
    inc index_dst

next:
    ; Move to next char in the input buffer
    inc index_src
    jmp loop

jump_table:
    .word default, remark, string, option1, option2, option3, option4, hexnum, binnum, decnum, symbol

.segment "VARS"
    state: .res 1
    non_whitespace_count: .res 1
    index_src: .res 1
    symbol_start: .res 1
    dp_count: .res 1
    index_dst: .res 1
    remarks_on: .res 1
.CODE
.endproc