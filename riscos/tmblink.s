;       IServer
;
;       Link driver for Gnome TMB
;
;       Richard Crook
;       February 1992

                AREA    |C$$code|, CODE, READONLY

                ;GET     ARMLib:Registers
                ;GET     ARMLib:Macros
                ;GET     ARMLib:APCS
                ;GET     ARMLib:OS
                ;GET     ARMLib:Podule

                GET     OSLibInclude:Hdr.Types
                GET     OSLibInclude:Hdr.OS
                GET     OSLibInclude:Hdr.Podule 

                EXPORT  OpenLink
                EXPORT  CloseLink
                EXPORT  |_ReadLink|
                EXPORT  |_WriteLink|
                EXPORT  ResetLink
                EXPORT  AnalyseLink
                EXPORT  TestError
                EXPORT  TestRead
                EXPORT  TestWrite

                KEEP

                ^       0,r0
Lk_ReadData     #       4
Lk_WriteData    #       4
Lk_ReadStatus   #       4
Lk_WriteStatus  #       4
Lk_CSR          #       4

CSR_Reset       *       1
CSR_Analyse     *       2
CSR_LkSpeed     *       4
CSR_LAReset     *       8
CSR_Error       *       16
CSR_IntMask     *       32
CSR_IntType     *       64

;
;               OpenLink
;
;               Ready the link associated with <name>
;               If <name> is null or blank, any free link may be used
;               Returns the link base address as a link id, or negative if none found
;
;               NB Current implementation ignores name parameter
;
;               On entry:       r0      Pointer to name
;               On exit:        r0      Return value
;
                                                        ;
OpenLink        ROUT                                    ;
                STMFD   sp!,{lr}                        ; Save registers
                                                        ;
                SUB     sp,sp,#16                       ; Make temporary workspace
                MOV     r3,#3                           ; Start from podule 3
                                                        ;
00              MOV     r2,sp                           ; Point to temporary buffer
                SWI     XPodule_ReadHeader              ; ... and read expansion card header
                BVS     %FT10                           ; ... skipping if not present
                                                        ;
                LDRB    r0,[sp,#3]                      ; Get product type (low)
                TEQ     r0,#&4B                         ; ... and check for TMB ID
                TEQNE   r0,#&24                         ; (PAP) ... or TLink ID
                LDREQ   r0,[sp,#4]                      ; ... checking second word if OK
                TEQEQ   r0,#&00000E00                   ;
                BEQ     %FT20                           ; Skip forward if card located
                                                        ;
10              SUBS    r3,r3,#1                        ; If card not found, try another
                BPL     %BT00                           ;
                                                        ;
                MOV     r0,#-1                          ; Open failed
                ADD     sp,sp,#16                       ;
                LDMFD   sp!,{pc}^                       ; ... so return
                                                        ;
20              MOV     r0,#&03300000                   ; Generate device address
                ORR     r0,r0,#&00042000                ;
                ORR     r0,r0,r3,LSL#14                 ;
                                                        ;
;                MOV     r1,#CSR_LAReset                 ; ... reset link adaptor
;                STR     r1,Lk_CSR                       ;
;                STR     r1,Lk_CSR                       ;
;                MOV     r1,#0                           ;
;                STR     r1,Lk_CSR                       ;
                ADD     sp,sp,#16                       ;
                LDMFD   sp!,{pc}^                       ; ... and return
                                                        ;
;
;               CloseLink
;
;               Close the active link <LinkId>
;               Returns 1 on success, or negative if the close failed
;
                                                        ;
CloseLink       ROUT                                    ;
                MOV     r0,#1                           ;
                MOVS    pc,lr                           ;
                                                        ;
;
;               ReadLink
;
;               Read `Count' chars into `Buffer' from the specified link.
;               LinkId is a vaild link identifier, opened with OpenLink.
;               Timeout' is a non negative integer representing tenths
;               of a second.  A `Timeout' of zero is an infinite timeout.
;               The timeout is for the complete operation.
;               If `Timeout' is positive then ReadLink may return having
;               read less that the number of chars asked for.
;               Returns the number of chars placed in `Buffer' (which may
;               be zero) or negative to indicate an error.
;
;               NB This implementation ignores the timeout parameter completely,
;                  and expects the high-level implementation to handle it.
;
;               On entry:       r0      LinkId
;                               r1      Buffer
;                               r2      Count
;                               r3      Timeout
;
                                                        ;
|_ReadLink|     ROUT                                    ;
                TEQ     r2,#0                           ; Check for zero bytes remaining
                MOVEQS  pc,lr                           ; ... and return if so
                                                        ;
                MOV     r3,lr                           ; Else save return address
                SWI     XOS_EnterOS                     ; ... and switch to SVC mode
                STMFD   sp!,{r1,r3-r4}                  ; Save registers on SVC stack
                                                        ;
                MOVS    r3,#256                         ; Loop timeout limit
                                                        ;
00              SUBEQS  r3,r3,#1                        ; Check for timeout exhausted
                BEQ     %FT10                           ; ... and exit if so
                                                        ;
                LDRB    r4,Lk_ReadStatus                ; Get link ready indicator
                TST     r4,#1                           ; Check for byte ready
                LDRNEB  r4,Lk_ReadData                  ; ... get byte if so
                STRNEB  r4,[r1],#1                      ; ... and store in buffer
                BEQ     %BT00                           ;
                SUBNES  r2,r2,#1                        ; ... then decrement byte count
                BNE     %BT00                           ; ... and loop if not complete
                                                        ;
10              LDMFD   sp!,{r2-r4}                     ; Recover registers
                SUB     r0,r1,r2                        ; Get count of bytes read
                MOVS    pc,r3                           ; ... and return
                                                        ;
;
;               WriteLink
;
;               Write `Count' chars from `Buffer' to the specified link.
;               LinkId is a vaild link identifier, opened with OpenLink.
;               `Timeout' is a non negative integer representing tenths
;               of a second.  A `Timeout' of zero is an infinite timeout.
;               The timeout is for the complete operation.
;               If `Timeout' is positive then WriteLink may return having
;               written less that the number of chars asked for.
;               Returns the number of chars actually written (which may
;               be zero) or negative to indicate an error.
;
;               NB This implementation ignores the timeout parameter completely,
;                  and expects the high-level implementation to handle it.
;
;               On entry:       r0      LinkId
;                               r1      Buffer
;                               r2      Count
;                               r3      Timeout
;
                                                        ;
|_WriteLink|    ROUT                                    ;
                TEQ     r2,#0                           ; Check for zero bytes remaining
                MOVEQS  pc,lr                           ; ... and return if so
                                                        ;
                MOV     r3,lr                           ; Else save return address
                SWI     XOS_EnterOS                     ; ... and switch to SVC mode
                STMFD   sp!,{r1,r3-r4}                  ; Save registers on SVC stack
                                                        ;
                MOVS    r3,#256                         ; Loop timeout limit
                                                        ;
00              SUBEQS  r3,r3,#1                        ; Check for timeout exhausted
                BEQ     %FT10                           ; ... and exit if so
                                                        ;
                LDRB    r4,Lk_WriteStatus               ; Get link ready indicator
                TST     r4,#1                           ; Check for byte ready
                LDRNEB  r4,[r1],#1                      ; ... get byte from buffer
                STRNEB  r4,Lk_WriteData                 ; ... and store to C012
                BEQ     %BT00                           ;
                SUBNES  r2,r2,#1                        ; ... then decrement byte count
                BNE     %BT00                           ; ... and loop if not complete
                                                        ;
10              LDMFD   sp!,{r2-r4}                     ; Recover registers
                SUB     r0,r1,r2                        ; Get count of bytes read
                MOVS    pc,r3                           ; ... and return
                                                        ;
;
;               ResetLink
;
;               Reset the subsystem associated with the specified link.
;               Returns 1 if the reset is successful, negative otherwise.
;
;               NB: Pause sets r1 to CSR_LkSpeed (20MHz) now -- RMJ
;
                                                        ;
Pause           ROUT                                    ;
                MOV     r1,#10*1024                     ; Pause about 5ms
00              SUBS    r1,r1,#1                        ;
                BNE     %BT00                           ;
                MOV     r1,#CSR_LkSpeed                 ; RMJ: fix to move to 20MHz painlessly
                MOVS    pc,lr                           ;
                                                        ;
ResetLink       ROUT                                    ;
                MOV     r3,lr                           ; Save return address
                SWI     XOS_EnterOS                     ;
                MOV     r2,lr                           ; Preserve r14_svc
                                                        ;
                MOV     r1,#CSR_Reset:OR:CSR_LAReset:OR:CSR_LkSpeed    ; Assert Host and C012 resets
                STRB    r1,Lk_CSR                       ;
                BL      Pause                           ;
                STRB    r1,Lk_CSR                       ; Deassert signals
                BL      Pause                           ;
                MOV     r0,#1                           ; Set return code
                MOV     lr,r2                           ; ... restore r14_svc
                MOVS    pc,r3                           ; ... and return
                                                        ;
;
;               Analyse the subsystem associated with the specified link.
;               Returns 1 if the analyse is successful, negative otherwise.
;
                                                        ;
AnalyseLink     ROUT                                    ;
                MOV     r3,lr                           ; Save return address
                SWI     XOS_EnterOS                     ;
                MOV     r2,lr                           ; Preserve r14_svc
                                                        ;
                MOV     r1,#CSR_Analyse:OR:CSR_LkSpeed  ; Assert analyse
                STRB    r1,Lk_CSR                       ;
                BL      Pause                           ;
                MOV     r1,#CSR_Analyse:OR:CSR_Reset:OR:CSR_LAReset:OR:CSR_LkSpeed
                STRB    r1,Lk_CSR                       ; Assert Host and C012 resets
                BL      Pause                           ;
                MOV     r1,#CSR_Analyse:OR:CSR_LkSpeed  ; Deassert resets
                STRB    r1,Lk_CSR                       ;
                BL      Pause                           ;
                STRB    r1,Lk_CSR                       ; Deassert analyse
                BL      Pause                           ;
                MOV     r0,#1                           ; Set return code
                MOVS    pc,r3                           ; ... and return
                                                        ;
;               TestError
;
;               Test the error status associated with the specified link.
;               Returns 1 if error is set, 0 if it is not and
;               negative to indicate an error.
;
                                                        ;
TestError       ROUT                                    ;
                MOV     r3,lr                           ; Save return address
                SWI     XOS_EnterOS                     ; ... switch to SVC mode
                LDRB    r0,Lk_CSR                       ; Get status byte
                MOV     r0,r0,LSR#4                     ; Isolate error bit
                AND     r0,r0,#1                        ;
                EOR     r0,r0,#1                        ; ... invert to get logical state
                MOVS    pc,r3                           ; ... and return
                                                        ;
;
;               TestRead
;
;               Test input status of the link.
;               Returns 1 if ReadLink will return one byte without timeout,
;               0 if it may not and negative to indicate an error.
;
                                                        ;
TestRead        ROUT                                    ;
                MOV     r3,lr                           ; Save return address
                SWI     XOS_EnterOS                     ; ... switch to SVC mode
                LDRB    r0,Lk_ReadStatus                ; Get link status byte
                AND     r0,r0,#1                        ;
                MOVS    pc,r3                           ; ... and return
                                                        ;
;
;               TestWrite
;
;               Test output status of the link.
;               Returns 1 if WriteLink will output one byte without timeout,
;               0 if it may not and negative to indicate an error.
;
                                                        ;
TestWrite       ROUT                                    ;
                MOV     r3,lr                           ; Save return address
                SWI     XOS_EnterOS                     ; ... switch to SVC mode
                LDRB    r0,Lk_WriteStatus               ; Get link status byte
                AND     r0,r0,#1                        ;
                MOVS    pc,r3                           ; ... and return
                                                        ;
;
;               End of file
;

                END
