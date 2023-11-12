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

.export token_init, token_get, token_names, token_addrl_tbl, token_addrh_tbl

;******************************************************************************
;Function name: token_init
;Purpose......: Copies token tables from BASIC ROM bank and gathers
;               metadata to support effective search of tokens
;Input........: Nothing
;Output.......: Nothing
;Error........: C = 1 if initialization failed
.proc token_init
    lda #1
    sta RAM_SEL

    ; Begin search at $c000
    stz token_ptr
    lda #$c0
    sta token_ptr+1

    ; Search for standard token table
    ldx #<std_identifier
    ldy #>std_identifier
    lda #6
    jsr token_hunt
    bcc :+
    jmp error
:   lda token_ptr
    sta token_std_head
    lda token_ptr+1
    sta token_std_head+1

    ; Search for extended statement table
    ldx #<ext_stmt_identifier
    ldy #>ext_stmt_identifier
    lda #6
    jsr token_hunt
    bcc :+
    jmp error
:   lda token_ptr
    sta token_ext_stmt_head
    lda token_ptr+1
    sta token_ext_stmt_head+1

    ; Search for extended function table
    ldx #<ext_func_identifier
    ldy #>ext_func_identifier
    lda #5
    jsr token_hunt
    bcc :+
    jmp error
:   lda token_ptr
    sta token_ext_func_head
    lda token_ptr+1
    sta token_ext_func_head+1

    ; Copy standard token table
    lda token_std_head
    sta token_ptr
    lda token_std_head+1
    sta token_ptr+1

    lda #<token_names
    sta TEMP1
    lda #>token_names
    sta TEMP1+1

    ldx #$ff
    ldy #$ff
    
    lda #0
    jsr token_copy
    tya
    dey
    sty token_std_last

    ; Copy extended statement table part 1
    ldx token_ext_stmt_head
    stx token_ptr
    ldx token_ext_stmt_head+1
    stx token_ptr+1
    ldx token_ext_func_head
    ldy token_ext_func_head+1
    jsr token_copy
    tya

    ; Copy extended statement table part 2
    inc token_ptr
    bne :+
    inc token_ptr+1

:   ldx token_ext_func_head
    ldy token_ext_func_head+1
    jsr token_copy
    tya
    dey
    sty token_ext_stmt_last

    ; Copy extended function table
    ldx #$ff
    ldy #$ff
    jsr token_copy
    dey
    sty token_ext_func_last
    rts
    
error:
    lda #1
    rts

std_identifier:
    .byt "en",$c4,"fo",$d2

ext_stmt_identifier:
    .byt "mo", 'n' + $80, "do", 's' + $80

ext_func_identifier:
    .byt "vpee", 'k' + $80
.endproc

;******************************************************************************
;Function name: token_hunt
;Purpose......: Searches BASIC ROM bank for a string
;Input........: X           String pointer (low)
;               Y           String pointer (high)
;               A           String len
;               token_ptr   Pointer to address in BASIC ROM bank where to
;                           start searching
;Output.......: token_ptr   Address where string was found
;Error........: C = 1       String was not found
.proc token_hunt 
    ; Save input
    stx TEMP1
    sty TEMP1+1
    sta len

    ; Go hunt!
    ldy #0
loop:
    bridge_fetch_basrom_index
    cmp (TEMP1),y
    bne nomatch
    iny
    cpy len
    bne loop

match:
    clc
    rts

nomatch:
    ldy #0
    inc token_ptr
    bne loop
    inc token_ptr+1
    lda token_ptr+1
    cmp #$c0
    bcs loop

notfound:
    sec
    rts

.segment "VARS"
    len: .res 1
.CODE
.endproc

;******************************************************************************
;Function name: token_copy
;Purpose......: Copies a token table into banked RAM. The copy will stop
;               if the value read from BASIC ROM is 0 or if it has reaced
;               the specified address limit
;Input........: X           Address where to stop copy (low)
;               Y           Address where to stop copy (high)
;               A           Index of first token
;               token_ptr   Pointer to BASIC ROM bank where to copy from
;               TEMP1       Pointer to RAM bank 1 where to copy to
;Output.......: Y           Index of next token
;Error........: Nothing
.proc token_copy
    ; Save input
    stx address
    sty address+1
    sta index

    ; Select RAM bank
    lda #1
    sta RAM_SEL

    ; Copy tokens
    ldy #0
    stz cs
loop:
    bridge_fetch_basrom_index
    beq exit
    sta (TEMP1),y
    iny
    pha
    clc
    and #%01111111
    adc cs
    sta cs
    pla
    bpl loop

token_end:
    sty len
    tya
    ldy index

    sta token_len_tbl,y             ; Store token len in table
    
    lda cs
    sta token_cs_tbl,y              ; Store token checksum in table
    clc
    lda TEMP1
    sta token_addrl_tbl,y           ; Store token addressL in table
    adc len                         ; Move input pointer to next token
    sta TEMP1
    lda TEMP1+1
    sta token_addrh_tbl,Y           ; Store token AddressH in table
    adc #0                          ; Move input pointer to next token
    sta TEMP1+1

next:
    inc index                       ; Prepare for next token
    ldy #0

    stz cs

    clc                             ; Move output pointer to next token
    lda token_ptr
    adc len
    sta token_ptr
    lda token_ptr+1
    adc #0
    sta token_ptr+1

    lda token_ptr+1
    cmp address+1
    bcc loop

    lda token_ptr
    cmp address
    bcc loop

exit:
    ; Exit and return index in Y register
    ldy index
    rts

.segment "VARS"
    address: .res 2
    index: .res 1
    len: .res 1
    cs: .res 1
.CODE
.endproc

;******************************************************************************
;Function name: token_get
;Purpose......: Returns token ID from a string
;Input........: X   Start index in file_buf
;               Y   End index in file_buf
;Output.......: A   Token
;               X   Extended token
;Error........: C = 1 if token not found
.proc token_get
    ; Store input
    stx index1
    sty index2

    ; Calculate len
    sec
    tya
    sbc index1
    ina
    sta len

    ; Calculate checksum
    stz cs
    ldx index1
:   lda file_buf,x
    cmp #$41
    bcc :+
    and #%11011111
:   clc
    adc cs
    sta cs
    cpx index2
    beq :+
    inx
    bra :--

    ; Select RAM bank 1
:   lda #1
    sta RAM_SEL

    ; Look for a matching token
    stz token_index
loop:
    ldx token_index

    lda token_cs_tbl,x
    cmp cs
    bne nomatch

    lda token_len_tbl,x
    cmp len
    bne nomatch

    ldy #0
    lda token_addrl_tbl,x
    sta TEMP1
    lda token_addrh_tbl,x
    sta TEMP1+1

    ldx index1

:   lda (TEMP1),y
    bmi eot
    sta temp
    lda file_buf,x
    cmp #$40
    bcc :+
    and #%11011111
:   cmp temp
    bne nomatch
    cpx index2
    bcs nomatch
    iny
    inx
    bra :--

eot:
    and #%01111111
    sta temp
    lda file_buf,x
    cmp #$40
    bcc :+
    and #%11011111
:   cmp temp
    bne nomatch

match:
    ldx token_index
    cpx token_std_last
    bcc std
    beq std

ext:
    cpx token_ext_stmt_last
    bcc extstmt
    beq extstmt

extfunc:
    txa
    sec
    sbc token_ext_stmt_last
    dea
    clc
    adc #$d0
    tax
    lda #$ce
    clc
    rts

extstmt:
    txa
    sec 
    sbc token_std_last
    dea
    clc
    adc #$80
    tax
    lda #$ce
    clc
    rts

std:
    txa
    clc
    adc #$80
    ldx #0

    clc
    rts
    
nomatch:
    inc token_index
    cpx token_ext_func_last
    beq :+
    bcs notfound
:   jmp loop

notfound:
    sec
    rts

.segment "VARS"
    index1: .res 1
    index2: .res 1
    cs: .res 1
    len: .res 1
    token_index: .res 1
    temp: .res 1
.CODE
.endproc

.ZEROPAGE
    token_ptr: .res 2

.segment "VARS"
    token_std_head: .res 2
    token_ext_stmt_head: .res 2
    token_ext_func_head: .res 2

    token_std_last: .res 1
    token_ext_stmt_last: .res 1
    token_ext_func_last: .res 1

.segment "RAM1"
    token_cs_tbl: .res $100
    token_len_tbl: .res $100
    token_addrl_tbl: .res $100
    token_addrh_tbl: .res $100
    token_names: .res $400
.CODE