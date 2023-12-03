; Exports needed for unit testing
.export line_init, line_pass1, line_pass2

; Line parsing states
LINE_DEFAULT_ELEMENT = 0
LINE_REMARK = 2
LINE_STRING = 4
LINE_HEXNUM = 6
LINE_BINNUM = 8
LINE_DECNUM = 10
LINE_SYMBOL = 12
LINE_DATA = 14

; Variables
.ZEROPAGE
    line_meta: .res 2
    line_code: .res 2

.segment "VARS"
    line_dstlin: .res 2
    line_option_rem: .res 1
.CODE

;******************************************************************************
;Function name: line_init
;Purpose......: Initializes the line parsing functions; to be called once
;               before pass1 and once before pass 2
;Input........: Nothing
;Output.......: Nothing
;Errors.......: None
.proc line_init
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

    ; Set default options
    stz line_option_rem ; Remove REms

    rts
.endproc

;******************************************************************************
;Function name: line_pass1
;Purpose......: Pass 1; looks for label defintions and stores them in
;               the symbol table
;Input........: file_buf    A line of source code, NULL terminated
;Output.......: Nothing
;Error........: C=1 if error occured

.proc line_pass1
    ; Init
    stz index_src
    stz index_dst
    stz state
    stz last_char

    ; Increment destination line
    inc line_dstlin
    bne :+
    inc line_dstlin+1

    ; Check destination line overflow
:   lda line_dstlin
    ora line_dstlin+1
    bne loop
    ldx #<msg_line_num_overflow
    ldy #>msg_line_num_overflow
    lda #RESPONSE_LINE_NUM_OVERFLOW
    clc
    jsr response_set
    sec
    rts

loop:
    ; Get next char
    ldx index_src
    lda file_buf,x
    bne switch_state
    
    ldx state
    cpx #LINE_SYMBOL
    beq switch_state

eol:
    ; Rewind destination line number if the line didn't output anything to BASIC memory
    lda index_dst
    bne :++

    lda line_dstlin
    bne :+
    dec line_dstlin+1
:   dec line_dstlin

    ; Set response value, C=0 => OK
:   clc
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

    ; Convert all alphabetic chars to the same case
:   cmp #$40
    bcc :+
    and #%11011111

    ; Option?
:   cmp #35
    bne symstrt
    ldx index_dst
    bne symstrt

    ldx index_src
    inx
    jsr option_parse
    cmp #0
    bne :+
    clc
    lda #RESPONSE_OPTION_UNKNOWN
    jsr response_set
    sec
    rts

:   cmp #$ff
    bne :+
    clc
    lda #RESPONSE_FILE_ERROR
    jsr response_set
    sec
    rts

:   cmp #$fe
    bne :+
    clc
    lda #RESPONSE_INVALID_PARAM
    jsr response_set
    sec
    rts

:   jmp eol
    
    ; Symbol start?
symstrt:   
    cmp #'a'
    bcs :+
    jmp fake_copy
:   cmp #'z'+1
    bcc :+
    jmp fake_copy

    ; Yes it's a symbol
:   stx symbol_start
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
    bcs :+
    jmp next

    ; Points are allowed in a symbol name, continue if found
:   cmp #'.'
    bne :+
    jmp next

    ; A symbol ended by a colon is a possible label declaration - needs further checking
:   cmp #':'
    beq symbol_chk

    ; Decimal numbers are allowed in symbol names, continue if found
    cmp #'0'
    bcc symbol_chk
    cmp #'9'+1
    bcs symbol_chk
    jmp next

symbol_chk:
    ldx symbol_start
    ldy index_src
    dey
    jsr symbol_find
    bcc symbol_exists
    
    lda index_dst
    bne symbol_end
    ldx index_src
    lda file_buf,x
    cmp #':'
    bne symbol_end

is_label:
    ldx symbol_start
    ldy index_src
    lda #0
    dey
    clc
    jsr symbol_add
    cmp #0
    beq is_label2
    cmp #1
    bne :+
    clc
    lda #RESPONSE_DUPLICATE_SYMBOL
    jsr response_set
    sec
    rts

:   cmp #2
    bne :+
    sec
    lda #RESPONSE_SYMTBL_FULL
    jsr response_set
    sec
    rts

:   cmp #3
    bne :+
    clc
    lda #RESPONSE_OUT_OF_VAR_NAMES
    jsr response_set
    sec
    rts

:   cmp #4
    bne is_label2
    clc
    lda #RESPONSE_SYMBOL_TOO_LONG
    jsr response_set
    sec
    rts

is_label2:
    stz state
    jmp next

symbol_exists:
    ldy index_dst
    bne :+

    cmp #0
    beq symbol_dup
:   cmp #2
    beq symbol_token
    bra symbol_end

symbol_dup:
    clc
    lda #RESPONSE_DUPLICATE_SYMBOL
    jsr response_set
    sec
    rts

symbol_token:
    cpx #$8f    ; REM statement?
    bne symbol_end
    
    lda line_option_rem
    bne symbol_end
    lda last_char
    cmp #':'
    bne :+
    dec index_dst
:   jmp eol

symbol_end:
    inc index_dst
    stz state
    jmp loop

fake_copy:
    ; In pass 2 this data will be copied to BASIC memory
    ; We need to keep track of it, in order to tell that this will output data to BASIC memory
    inc index_dst
    sta last_char

next:
    inc index_src
    jmp loop

.segment "VARS"
    index_src: .res 1
    index_dst: .res 1
    state: .res 1
    symbol_start: .res 1
    last_char: .res 1
.CODE
.endproc

;******************************************************************************
;Function name: line_pass2
;Purpose......: Load pass 2; tokenizes the source code and replaces long
;               variable names with short names supported by BASIC v 2
;Input........: file_buf    A line of source code, NULL terminated
;Output.......: Nothing
;Output.......: Nohting
;Error........: C=1 if error occured
.proc line_pass2
    ; Init
    stz index_src
    stz index_dst
    stz state
    stz last_token

    ; Increment destination line
    inc line_dstlin
    bne loop
    inc line_dstlin+1

loop:
    ; Get next char from file buffer
    ldx index_src
    lda file_buf,x
    bne switch_state

    ldx state
    cpx #LINE_SYMBOL
    beq switch_state

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
    ; Exit with status C=0 => OK
    clc
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

    ; String
:   cmp #34 ; Double quote
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
    bne ishex
    ldx index_dst
    bne ishex
    
    ldx index_src
    inx
    jsr option_parse
    cmp #0
    bne :+
    clc
    lda #RESPONSE_OPTION_UNKNOWN
    jsr response_set
    sec
    rts

:   cmp #$ff
    bne :+
    clc
    lda #RESPONSE_FILE_ERROR
    jsr response_set
    sec
    rts

:   cmp #$fe
    bne :+
    clc
    lda #RESPONSE_INVALID_PARAM
    jsr response_set
    sec
    rts

:   jmp eol

    ; Hexadecimal number - Starts with a $ sign
ishex:
    cmp #'$'
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
    jsr symbol_find
    bcs :+
    
    txa
    ldy index_dst
    sta (line_code),y
    inc index_dst
    jmp next    

    ; Other input
:   ldx index_src
    lda file_buf,x
    cmp #':'
    bne :+
    stz last_token
    stz last_token+1
:   jmp copy

remark:
    ldx line_option_rem
    beq :+
    jmp copy
:   jmp next

string:
    cmp #34 ; Double quote
    bne :+
    ldx #LINE_DEFAULT_ELEMENT
    stx state
:   jmp copy

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

    ; Decimal numbers are allowed in symbol names, continue if found
    cmp #'0'
    bcc symbol_check1
    cmp #'9'+1
    bcs symbol_check1

symbol_cont:
    ; Continue, get next char
    jmp next

symbol_check1:
    ; Check if it's a symbol (including the current char)
    ldx symbol_start
    ldy index_src
    jsr symbol_find
    bcc symbol_found

symbol_check2:
    dec index_src
    ldx symbol_start
    ldy index_src
    jsr symbol_find
    bcs newvar ; Previously unknown symbol, treat as new variable
    
symbol_found:
    ; If last token was GOTO or GOSUB, the symbol must be a label
    pha

    cmp #0
    beq :++

    lda last_token
    cmp #$89
    beq :+
    cmp #$8d
    bne :++

:   pla
    clc
    lda #RESPONSE_LABEL_EXPECTED
    jsr response_set
    sec
    rts

:   pla
    cmp #1
    beq insvar
    cmp #2
    bne :+ 
    jmp instoken

    ; It's a label, insert its line number, if not at start of line
:   lda index_dst
    bne :+
    inc index_src
    bra :+++

:   lda #0
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
    lda #1 ; variable
    jsr symbol_add
    cmp #0
    beq insvar
    cmp #1
    bne :+
    clc
    lda #RESPONSE_DUPLICATE_SYMBOL
    jsr response_set
    sec
    rts

:   cmp #2
    bne :+
    sec
    lda #RESPONSE_SYMTBL_FULL
    jsr response_set
    sec
    rts

:   cmp #3
    bne :+
    clc
    lda #RESPONSE_OUT_OF_VAR_NAMES
    jsr response_set
    sec
    rts

:   cmp #4
    bne insvar
    clc
    lda #RESPONSE_SYMBOL_TOO_LONG
    jsr response_set
    sec
    rts
    
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
    inc index_src
    ldx index_src
    lda file_buf,x
    cmp #'$'
    beq copy
    cmp #'%'
    beq copy
    jmp loop

instoken:
    stx last_token
    sty last_token+1

    ; Insert first byte
    phy
    ldy index_dst
    txa
    sta (line_code),y
    inc index_dst
    
    ; Insert extended token, if applicable
    pla
    beq :+
    iny
    sta (line_code),y
    inc index_dst
    bra :+++

:   cpx #$8f ; REM statement
    bne :++
    lda #LINE_REMARK
    sta state
    lda line_option_rem
    bne :+
    dec index_dst
    ldy index_dst
    beq :+
    dey
    lda (line_code),y
    cmp #':'
    bne :+
    dec index_dst

:   jmp next

:   cpx #$83 ; DATA statement
    bne :+
    lda #LINE_DATA
    sta state
    stz quotes_count
    jmp next

:   stz state
    jmp next

data:
    tax

    cmp #34 ; Double quote?
    bne :+
    inc quotes_count
    bra :++

:   lda quotes_count
    and #1
    bne :+

    cpx #':'
    bne :+
    stz state

:   txa
    jmp copy

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
    .word default, remark, string, hexnum, binnum, decnum, symbol, data

.segment "VARS"
    state: .res 1
    index_src: .res 1
    symbol_start: .res 1
    dp_count: .res 1
    index_dst: .res 1
    last_token: .res 2
    quotes_count: .res 1
    option_start: .res 1
.CODE
.endproc