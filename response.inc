
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

RESPONSE_OK                     = 0
RESPONSE_IO_ERROR               = 1
RESPONSE_DISK_ERROR             = 2
RESPONSE_SRC_LINE_NUM_OVF       = 3
RESPONSE_DST_LINE_NUM_OVF       = 4
RESPONSE_LINE_TOO_LONG          = 5
RESPONSE_MISSING_LABEL          = 6
RESPONSE_DUPLICATE_SYMBOL       = 7
RESPONSE_SYMBOL_TOO_LONG        = 8
RESPONSE_OUTOFVARNAMES          = 9
RESPONSE_PARSE_ERROR            = 10
RESPONSE_SYMTBL_FULL            = 11

;******************************************************************************
;Function name: response_set
;Purpose......: Sets application response code and message
;Input........: A = return code
;                   0       = OK
;                   1-255   = Error code
;               X/Y = Pointer to response text message
;               C = 1, no line number in output
;Output.......: Response code is stored in R1L
;               Response message stored in bank 0, $bf00..bfff
;               An error replaces an earlier warning
;               A warning is ignored if there is already a response code and
;               message stored
;Errors.......: A = 1 if error occured
.proc response_set
    ; Save input
    sta KERNAL_R1
    stz show_line
    bcc msg_cpy
    inc show_line
                            
msg_cpy:
    ; Select RAM bank 0
    lda RAM_SEL
    pha
    stz RAM_SEL

    ; Set pointers to message source and destination
    stx TEMP1
    sty TEMP1+1
    stz TEMP2
    lda #$bf
    sta TEMP2+1
    ldy #0

    ; Copy null terminated string
:   lda (TEMP1),y
    beq line_num
    sta (TEMP2),y
    iny
    bra :-

line_num:
    ; Clear line number output
    stz KERNAL_R1+1
    stz KERNAL_R2
    stz KERNAL_R2+1

    ; Skip if C was 1 => show_line = 1
    lda show_line
    bne exit

    ; Set pointer to end of output string
    clc
    tya
    adc TEMP2
    sta TEMP2
    lda TEMP2+1
    adc #0
    sta TEMP2+1

    ; Store line number
    ldx line_srclin
    stx KERNAL_R1+1
    ldy line_srclin+1
    sty KERNAL_R2
    lda line_srclin+2
    sta KERNAL_R2+1

    ; Convert line number to string
    jsr util_bin_to_bcd
    jsr util_bcd_to_str
    stx TEMP1
    sty TEMP1+1
    
    ; Output line number to message
    ldy #0
:   lda (TEMP1),y
    sta (TEMP2),y
    beq exit
    iny
    bra :-

exit:
    pla
    sta RAM_SEL
    rts

.segment "VARS"
    show_line: .res 1
.CODE
.endproc
