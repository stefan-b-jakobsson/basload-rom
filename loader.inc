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

;******************************************************************************
;Function name: loader_run
;Purpose......: Loads the source file
;Input........: Nothing
;Output.......: Nothing
;Errors.......: Nothing
.proc loader_run
    ;Set default response code = OK
    lda #0
    ldx #<msg_ok
    ldy #>msg_ok
    sec
    jsr response_set
    
    ;Prepare pass 1
    jsr symbol_init
    jsr token_init
    jsr line_init

    jsr file_open
    cmp #0
    bne err1a

    ;Read and process each line
:   jsr loader_readln
    cmp #1
    beq eof1
    cmp #2
    beq err1a
    cmp #3
    beq err1b

    jsr line_pass1
    bcc :-

err1a:
    jsr file_close
    jsr file_set_disk_status
    rts

err1b:
    jsr file_close
    clc
    ldx #<msg_line_too_long
    ldy #>msg_line_too_long
    lda #RESPONSE_LINE_TOO_LONG
    jmp response_set

eof1:
    jsr line_pass1
    bcs err1a
    
    ;Prepare pass 2
    jsr line_init

    ;Open source file
    jsr file_close
    jsr file_open

    ;Read and process each line again
pass2_loop:
    jsr loader_readln
    cmp #1
    beq eof2
    cmp #2
    bcs err2

    jsr line_pass2
    bcc pass2_loop

err2:
    jsr file_close
    jsr file_set_disk_status
    rts

eof2:
    jsr line_pass2
    bcs err2

    ;We're done
    jsr file_close

    ;Set BASIC pointers
    lda line_code
    sta KERNAL_VARTAB
    sta KERNAL_ARYTAB
    sta KERNAL_STREND
    
    lda line_code+1
    sta KERNAL_VARTAB+1
    sta KERNAL_ARYTAB+1
    sta KERNAL_STREND+1
    rts

.endproc

;******************************************************************************
;Function name: loader_readlin
;Purpose......: Reads one line from the current input source and stores it
;               into file_buffer
;Input........: Nothing
;Output.......: A = 0 if OK but not EOF
;               A = 1 if OK and EOF
;Errors.......: A = 2 if error reading line
;               A = 3 if line longer than 250 characters
.proc loader_readln
    ;For now there is only one possible input source
    jmp file_readln
.endproc
