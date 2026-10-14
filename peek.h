static unsigned char getstate[] =
{0x2F,				/* BYTE END-START */
 0xB5,				/* START:  AJW  5 */
 0x90,				/* CALL 0         */
 0x14,				/* LDLP 4         */
 0x23, 0xFE,			/* SAVEH          */
 0x16,				/* LDLP 6         */
 0x23, 0xFD,			/* SAVEL          */
 0x24, 0xF2,			/* MINT           */
 0x21, 0xFC,			/* STLF           */
 0x24, 0xF2,			/* MINT           */
 0x21, 0xF8,			/* STHF           */
 0x48,				/* LDC  8         */
 0x23, 0xF4,			/* BCNT           */
 0xD0,				/* STL  0         */
 0x10,				/* LDLP 0         */
 0x73,				/* LDL  3         */
 0x60, 0x5C,			/* LDNLP -4       */
 0x42,				/* LDC  2         */
 0xFB,				/* OUT            */
 0x25, 0xF9,			/* TESTHALTERR    */
 0x44,				/* LDC  4         */
 0x24, 0xF1,			/* SHL            */
 0x22, 0xF9,			/* TESTERR        */
 0x24, 0xFB,			/* OR             */
 0xD0,				/* STL  0         */
 0x10,				/* LDLP 0         */
 0x73,				/* LDL  3         */
 0x60, 0x5C,			/* LDNLP -4       */
 0x48,				/* LDC  8         */
 0x23, 0xF4,			/* BCNT           */
 0xFB,				/* OUT            */
 0x21, 0x2F, 0xFF};		/* OPR $1FF       */
 				/* END:           */
