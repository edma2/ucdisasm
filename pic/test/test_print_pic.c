#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <bytestream.h>
#include <disasmstream.h>
#include <instruction.h>
#include <file/debug.h>
#include <printstream_file.h>

#include <pic/pic_support.h>
#include <pic/pic_instruction_set.h>

/******************************************************************************/
/* PIC Print Stream Test Instrumentation */
/******************************************************************************/

static int test_printstream(char *name, int subarch, uint8_t *test_data, uint32_t *test_address, unsigned int test_len, int flags) {
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

    /* Setup the PIC Disasm Stream */
    ds.in = &bs;
    if (subarch == PIC_SUBARCH_BASELINE) {
        ds.stream_init = disasmstream_pic_baseline_init;
        ds.stream_close = disasmstream_pic_baseline_close;
        ds.stream_read = disasmstream_pic_baseline_read;
    } else if (subarch == PIC_SUBARCH_MIDRANGE) {
        ds.stream_init = disasmstream_pic_midrange_init;
        ds.stream_close = disasmstream_pic_midrange_close;
        ds.stream_read = disasmstream_pic_midrange_read;
    } else if (subarch == PIC_SUBARCH_MIDRANGE_ENHANCED) {
        ds.stream_init = disasmstream_pic_midrange_enhanced_init;
        ds.stream_close = disasmstream_pic_midrange_enhanced_close;
        ds.stream_read = disasmstream_pic_midrange_enhanced_read;
    }

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
/* PIC Print Stream Unit Tests */
/******************************************************************************/

int test_print_pic_unit_tests(void) {
    int i;
    int numTests = 0, passedTests = 0;

    /* Sample Program */
    /* rjmp l1; l1: ser r16; out 0x17, r16; dec r16; rjmp l2; jmp 0x2abab4;
     * cbi 0x12, 7; ldi r16, 0xaf; ret; nop; st Y, r2; st Y+, r2; st -Y, r2;
     * std y+5, r2; l2: st X, r3; st X+, r3; st -X, r3; st Y, r4; st Y+, r4;
     * st -Y, r4; std y+5, r4; .word 0xffff; .byte 0xff */
    uint8_t d[] = {0x00, 0xc0, 0x0f, 0xef, 0x07, 0xbb, 0x0a, 0x95, 0x0a, 0xc0, 0xad, 0x94, 0x5a, 0x5d, 0x97, 0x98, 0x0f, 0xea, 0x08, 0x95, 0x00, 0x00, 0x28, 0x82, 0x29, 0x92, 0x2a, 0x92, 0x2d, 0x82, 0x3c, 0x92, 0x3d, 0x92, 0x3e, 0x92, 0x48, 0x82, 0x49, 0x92, 0x4a, 0x92, 0x4d, 0x82, 0xff, 0xff, 0xff};
    uint32_t a[sizeof(d)]; for (i = 0; i < sizeof(d); i++) a[i] = i;

    /* Check typical options */
    {
        int flags = PRINT_FLAG_ADDRESSES | PRINT_FLAG_DESTINATION_COMMENT | PRINT_FLAG_DATA_HEX | PRINT_FLAG_OPCODES;

        if (test_printstream("Typical Options", PIC_SUBARCH_MIDRANGE_ENHANCED, &d[0], &a[0], sizeof(d), flags) == 0)
            passedTests++;
        numTests++;
    }

    /* Check data type bin */
    {
        int flags = PRINT_FLAG_ADDRESSES | PRINT_FLAG_DESTINATION_COMMENT | PRINT_FLAG_DATA_BIN | PRINT_FLAG_OPCODES;

        if (test_printstream("Data Type Bin", PIC_SUBARCH_MIDRANGE_ENHANCED, &d[0], &a[0], sizeof(d), flags) == 0)
            passedTests++;
        numTests++;
    }

    /* Check data type dec */
    {
        int flags = PRINT_FLAG_ADDRESSES | PRINT_FLAG_DESTINATION_COMMENT | PRINT_FLAG_DATA_DEC | PRINT_FLAG_OPCODES;

        if (test_printstream("Data Type Dec", PIC_SUBARCH_MIDRANGE_ENHANCED, &d[0], &a[0], sizeof(d), flags) == 0)
            passedTests++;
        numTests++;
    }

    /* Check no original opcode */
    {
        int flags = PRINT_FLAG_ADDRESSES | PRINT_FLAG_DESTINATION_COMMENT | PRINT_FLAG_DATA_HEX;

        if (test_printstream("No Original Opcode", PIC_SUBARCH_MIDRANGE_ENHANCED, &d[0], &a[0], sizeof(d), flags) == 0)
            passedTests++;
        numTests++;
    }

    /* Check no addresses, no destination comments */
    {
        int flags = PRINT_FLAG_DATA_HEX;

        if (test_printstream("No Addresses, No Destination Comments", PIC_SUBARCH_MIDRANGE_ENHANCED, &d[0], &a[0], sizeof(d), flags) == 0)
            passedTests++;
        numTests++;
    }

    /* Check assembly output */
    {
        int flags = PRINT_FLAG_ASSEMBLY | PRINT_FLAG_DESTINATION_COMMENT | PRINT_FLAG_DATA_HEX;

        if (test_printstream("Assembly", PIC_SUBARCH_MIDRANGE_ENHANCED, &d[0], &a[0], sizeof(d), flags) == 0)
            passedTests++;
        numTests++;
    }

    printf("%d / %d tests passed.\n\n", passedTests, numTests);

    if (passedTests == numTests)
        return 0;

    return -1;
}

