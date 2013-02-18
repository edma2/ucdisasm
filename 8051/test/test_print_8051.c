#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <bytestream.h>
#include <disasmstream.h>
#include <instruction.h>
#include <file/debug.h>
#include <printstream_file.h>

#include <8051/8051_support.h>

/******************************************************************************/
/* 8051 Print Stream Test Instrumentation */
/******************************************************************************/

static int test_printstream(char *name, uint8_t *test_data, uint32_t *test_address, unsigned int test_len, int flags) {
    struct ByteStream bs;
    struct DisasmStream ds;
    struct PrintStream ps;
    int ret;

    printf("Running test \"%s\"\n", name);

    /* Setup a Debug Byte Stream */
    bs.in = NULL;
    bs.stream_init = bytestream_debug_init;
    bs.stream_close = bytestream_debug_close;
    bs.stream_read = bytestream_debug_read;

    /* Setup the 8051 Disasm Stream */
    ds.in = &bs;
    ds.stream_init = disasmstream_8051_init;
    ds.stream_close = disasmstream_8051_close;
    ds.stream_read = disasmstream_8051_read;

    /* Setup the Print Stream */
    ps.in = &ds;
    ps.stream_init = printstream_file_init;
    ps.stream_close = printstream_file_close;
    ps.stream_read = printstream_file_read;

    /* Initialize the stream */
    ret = ps.stream_init(&ps, flags);
    printf("\tps.stream_init(): %d\n", ret);
    if (ret < 0) {
        printf("\t\tError: %s\n", ps.error);
        return -1;
    }

    /* Load the Byte Stream with the test vector */
    ((struct bytestream_debug_state *)bs.state)->data = test_data;
    ((struct bytestream_debug_state *)bs.state)->address = test_address;
    ((struct bytestream_debug_state *)bs.state)->len = test_len;

    /* Read disassembled instructions from the print stream until EOF */
    while ( (ret = ps.stream_read(&ps, stdout)) != STREAM_EOF ) {
        if (ret != STREAM_EOF && ret < 0) {
            printf("\tps.stream_read(): %d\n", ret);
            printf("\t\tError: %s\n", ps.error);
            return -1;
        }
    }

    /* Close the stream */
    ret = ps.stream_close(&ps);
    printf("\tps.stream_close(): %d\n", ret);
    if (ret < 0) {
        printf("\t\tError: %s\n", ps.error);
        return -1;
    }

    printf("\n");

    return 0;
}

/******************************************************************************/
/* 8051 Print Stream Unit Tests */
/******************************************************************************/

int test_print_8051_unit_tests(void) {
    int i;
    int numTests = 0, passedTests = 0;

    /* Check Sample Program */
    /* org 000h; nop; inc A; dec 023h; inc @R0; dec @R1; inc R5; label1: add A,
     * #0aah; addc A, 023h; orl A, @R0; anl A, @R1; xrl A, R5; rr A; rrc A; dec
     * A; cpl A; mov A, #0aah; mov 023h, #0bbh; mov @R0, #0cch; mov @R1, #0ddh;
     * mov R1, #0eeh; mov 023h, 025h; mov 023h, R1; mov 023h, @R1; orl C,
     * /022h; ajmp label2; lcall label2; jc label1; jz label1; djnz R2, label1;
     * cjne A, #055h, label1; cjne A, 023h, label1; cjne @R0, #055h, label1;
     * cjne @R1, #055h, label1; cjne R4, #055h, label1; push 023h; pop 020h;
     * ret; reti; jmp @A+DPTR; inc DPTR; div AB; anl C, 043h; movx @DPTR, A;
     * movx A, @DPTR; movc A, @A+DPTR; movc A, @A+PC; jmp @A+DPTR; org 501h;
     * label2: nop; inc A; dec 023h; inc @R0; dec @R1; inc R5; */
    uint8_t d[] = {0x00, 0x04, 0x15, 0x23, 0x06, 0x17, 0x0d, 0x24, 0xaa, 0x35, 0x23, 0x46, 0x57, 0x6d, 0x03, 0x13, 0x14, 0xf4, 0x74, 0xaa, 0x75, 0x23, 0xbb, 0x76, 0xcc, 0x77, 0xdd, 0x79, 0xee, 0x85, 0x25, 0x23, 0x89, 0x23, 0x87, 0x23, 0xa0, 0x22, 0xa1, 0x01, 0x12, 0x05, 0x01, 0x40, 0xda, 0x60, 0xd8, 0xda, 0xd6, 0xb4, 0x55, 0xd3, 0xb5, 0x23, 0xd0, 0xb6, 0x55, 0xcd, 0xb7, 0x55, 0xca, 0xbc, 0x55, 0xc7, 0xc0, 0x23, 0xd0, 0x20, 0x22, 0x32, 0x73, 0xa3, 0x84, 0x82, 0x43, 0xf0, 0xe0, 0x93, 0x83, 0x73, 0x00, 0x04, 0x15, 0x23, 0x06, 0x17, 0x0d,};
    uint32_t a[sizeof(d)];
    for (i = 0; i < sizeof(d)-7; i++) a[i] = i;
    for (; i < sizeof(d); i++) a[i] = 0x501 + i - (sizeof(d)-7);

    /* Check typical options */
    {
        int flags = PRINT_FLAG_ADDRESSES | PRINT_FLAG_DESTINATION_COMMENT | PRINT_FLAG_DATA_HEX | PRINT_FLAG_OPCODES;

        if (test_printstream("8051 Typical Options", (uint8_t *)d, (uint32_t *)a, sizeof(d), flags) == 0)
            passedTests++;
        numTests++;
    }

    /* Check data type bin */
    {
        int flags = PRINT_FLAG_ADDRESSES | PRINT_FLAG_DESTINATION_COMMENT | PRINT_FLAG_DATA_BIN | PRINT_FLAG_OPCODES;

        if (test_printstream("8051 Data Type Bin", (uint8_t *)d, (uint32_t *)a, sizeof(d), flags) == 0)
            passedTests++;
        numTests++;
    }

    /* Check data type dec */
    {
        int flags = PRINT_FLAG_ADDRESSES | PRINT_FLAG_DESTINATION_COMMENT | PRINT_FLAG_DATA_DEC | PRINT_FLAG_OPCODES;

        if (test_printstream("8051 Data Type Dec", (uint8_t *)d, (uint32_t *)a, sizeof(d), flags) == 0)
            passedTests++;
        numTests++;
    }

    /* Check no original opcode */
    {
        int flags = PRINT_FLAG_ADDRESSES | PRINT_FLAG_DESTINATION_COMMENT | PRINT_FLAG_DATA_HEX;

        if (test_printstream("8051 No Original Opcode", (uint8_t *)d, (uint32_t *)a, sizeof(d), flags) == 0)
            passedTests++;
        numTests++;
    }

    /* Check no addresses, no destination comments */
    {
        int flags = PRINT_FLAG_DATA_HEX;

        if (test_printstream("8051 No Addresses, No Destination Comments", (uint8_t *)d, (uint32_t *)a, sizeof(d), flags) == 0)
            passedTests++;
        numTests++;
    }

    /* Check assembly output */
    {
        int flags = PRINT_FLAG_ASSEMBLY | PRINT_FLAG_DESTINATION_COMMENT | PRINT_FLAG_DATA_HEX;

        if (test_printstream("8051 Assembly", (uint8_t *)d, (uint32_t *)a, sizeof(d), flags) == 0)
            passedTests++;
        numTests++;
    }

    printf("%d / %d tests passed.\n\n", passedTests, numTests);

    if (passedTests == numTests)
        return 0;

    return -1;
}

