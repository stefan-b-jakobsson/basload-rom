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
    jsr response_set
    
    ;Prepare pass 1
    jsr token_init
    jsr line_init
    jsr label_init

    jsr file_open
    cmp #0
    bne err1

    ;Read and process each line
:   jsr loader_readln
    cmp #1
    beq eof1
    cmp #2
    beq err1

    jsr line_pass1
    cmp #0
    beq :-

err1:
    jsr file_close
    jsr file_set_disk_status
    jmp line_init

eof1:
    jsr line_pass1
    cmp #0
    bne err1
    
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
    beq err2

    jsr line_pass2
    beq pass2_loop

err2:
    jsr file_close
    jsr file_set_disk_status
    jmp line_init

eof2:
    jsr line_pass2
    cmp #0
    bne err2

    ;We're done
    jsr file_close
    rts

.endproc

;******************************************************************************
;Function name: loader_readlin
;Purpose......: Reads one line from the current input source and stores it
;               into file_buffer
;Input........: Nothing
;Output.......: Nothing
;Errors.......: Nothing
.proc loader_readln
    ;For now there is only one possible input source
    jmp file_readln
.endproc