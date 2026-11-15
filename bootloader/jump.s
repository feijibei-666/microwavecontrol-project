        AREA    |.text|, CODE, READONLY
        THUMB
        PRESERVE8
        EXPORT  Boot_Jump

Boot_Jump PROC
        ; R0 = APP initial MSP, R1 = Thumb Reset_Handler.
        ; Do not return to C after changing the active stack.
        MSR     MSP, R0
        CPSIE   i
        BX      R1
        ENDP
        END
