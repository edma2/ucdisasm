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

    /* Sample Midrange Enhanced Program */
    /* clrw; clrf 0x15; incf 5, f; movf 0x15, W; bsf 0x15, 3; btfsc 0x15, 2;
     * a: andlw 0xfe; call 0x600; goto 0x604; sleep; clrwdt;
     * lslf 0x15, f; addwfc 0x15, W; decfsz 0x15, f; movlp 0x7f; bra a; brw;
     * reset; addfsr FSR0, 0x0a; moviw ++FSR1; moviw --FSR1; moviw FSR0++;
     * moviw FSR0--; moviw 5[FSR1]; end */
    uint8_t d[] = {0x03, 0x01, 0x95, 0x01, 0x85, 0x0a, 0x15, 0x08, 0x95, 0x15, 0x15, 0x19, 0xfe, 0x39, 0x00, 0x26, 0x04, 0x2e, 0x63, 0x00, 0x64, 0x00, 0x95, 0x35, 0x15, 0x3d, 0x95, 0x0b, 0xff, 0x31, 0xf6, 0x33, 0x0b, 0x00, 0x01, 0x00, 0x0a, 0x31, 0x14, 0x00, 0x15, 0x00, 0x12, 0x00, 0x13, 0x00, 0x45, 0x3f};
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

