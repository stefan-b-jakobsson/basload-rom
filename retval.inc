;******************************************************************************
;Function name: response_set
;Purpose......: Sets application response code and message
;Input........: A = return code
;                   0       = OK
;                   1-127   = Errors
;                   128-255 = Warnings
;               X/Y = Pointer to response text message
;Output.......: Response code is stored in R1L
;               Response message stored in bank 0, $bf00..bfff
;               An error replaces an earlier warning
;               A warning is ignored if there is already a response code and
;               message stored
;Errors.......: A = 1 if error occured
.proc response_set
    cmp #0
    bpl error
 
warn:                 
    pha
    lda KERNAL_R1
    beq :+
    pla
    rts
 
:   pla
 
error:
    sta KERNAL_R1
                            
msg:
    lda RAM_SEL
    pha
    stz RAM_SEL

    stx TEMP1
    sty TEMP1+1
    stz TEMP2
    lda #$bf
    sta TEMP2+1
    ldy #0

:   lda (TEMP1),y
    beq line_num
    sta (TEMP2),y
    iny
    bra :-

line_num:
    lda KERNAL_R1
    beq ok_message

    clc
    tya
    adc TEMP2
    sta TEMP2
    lda TEMP2+1
    adc #0
    sta TEMP2+1

    ldx line_srclin
    stx KERNAL_R1+1
    ldy line_srclin+1
    sty KERNAL_R2
    lda line_srclin+2
    sta KERNAL_R2+1

    jsr util_bin_to_bcd
    jsr util_bcd_to_str
    stx TEMP1
    sty TEMP1+1
    
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

ok_message:
    iny
    lda #0
    sta (TEMP2),y
    bra exit
    
.endproc

RESPONSE_OK                     = 0
RESPONSE_IO_ERROR               = 1
RESPONSE_DISK_ERROR             = 2
RESPONSE_SRC_LINE_NUM_OVF       = 3
RESPONSE_DST_LINE_NUM_OVF       = 4
RESPONSE_LINE_TOO_LONG          = 5
RESPONSE_MISSING_LABEL          = 6
RESPONSE_DUPLICATE_LABEL        = 7
RESPONSE_PARSE_ERROR            = 8
RESPONSE_SYMTBL_FULL            = 9

RESPONSE_LONG_LABEL             = 128
