#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <bytestream.h>
#include <disasmstream.h>
#include <file/debug.h>
#include <instruction.h>

#include <pic/pic_support.h>
#include <pic/pic_instruction_set.h>

/******************************************************************************/
/* PIC Disasm Stream Test Instrumentation */
/******************************************************************************/

static int test_disasmstream(int subarch, uint8_t *test_data, uint32_t *test_address, unsigned int test_len, struct instruction *output_instrs, unsigned int *output_len) {
    struct ByteStream bs;
    struct DisasmStream ds;
    int ret;

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

    /* Initialize the stream */
    ret = ds.stream_init(&ds);
    printf("\tds.stream_init(): %d\n", ret);
    if (ret < 0) {
        printf("\t\tError: %s\n", ds.error);
        return -1;
    }

    /* Load the Byte Stream with the test vector */
    ((struct bytestream_debug_state *)bs.state)->data = test_data;
    ((struct bytestream_debug_state *)bs.state)->address = test_address;
    ((struct bytestream_debug_state *)bs.state)->len = test_len;

    *output_len = 0;

    for (; ret != STREAM_EOF; ) {
        /* Disassemble an instruction */
        ret = ds.stream_read(&ds, output_instrs++);
        if (ret == 0) {
            *output_len = *output_len + 1;
        } else if (ret != STREAM_EOF && ret < 0) {
            printf("\tds.stream_read(): %d\n", ret);
            printf("\t\tError: %s\n", ds.error);
            break;
        }
    }

    /* Close the stream */
    ret = ds.stream_close(&ds);
    printf("\tds.stream_close(): %d\n", ret);
    if (ret < 0) {
        printf("\t\tError: %s\n", ds.error);
        return -1;
    }

    printf("\n");

    return 0;
}

static int test_disasm_pic_unit_test_run(char *name, int subarch, uint8_t *test_data, uint32_t *test_address, unsigned int test_len, struct picInstructionDisasm *expected_instructionDisasms, unsigned int expected_len) {
    struct picInstructionDisasm *instructionDisasm;
    struct instruction instrs[32];
    unsigned int len;
    int ret, i, j;
    int success;

    printf("Running test \"%s\"\n", name);

    /* Run the Disasm Stream on the test vectors */
    ret = test_disasmstream(subarch, test_data, test_address, test_len, &instrs[0], &len);
    if (ret != 0) {
        printf("\tFAILURE ret != 0\n\n");
        return -1;
    }
    printf("\tSUCCESS ret == 0\n");

    /* Compare number of disassembled instructions */
    if (len != expected_len) {
        printf("\tFAILURE len != expected_len\n\n");
        return -1;
    }
    printf("\tSUCCESS len == expected_len\n");

    success = 1;

    /* Compare each disassembled instruction */
    for (i = 0; i < expected_len; i++) {
        instructionDisasm = (struct picInstructionDisasm *)instrs[i].instructionDisasm;

        /* Compare instruction address */
        if (instructionDisasm->address != expected_instructionDisasms[i].address) {
            printf("\tFAILURE instr %d address:\t0x%04x, \texpected 0x%04x\n", i, instructionDisasm->address, expected_instructionDisasms[i].address);
            success = 0;
        } else {
            printf("\tSUCCESS instr %d address:\t0x%04x, \texpected 0x%04x\n", i, instructionDisasm->address, expected_instructionDisasms[i].address);
        }

        /* Compare instruction identified */
        if (instructionDisasm->instructionInfo != expected_instructionDisasms[i].instructionInfo) {
            printf("\tFAILURE instr %d:  \t\t%s, \t\texpected %s", i, instructionDisasm->instructionInfo->mnemonic, expected_instructionDisasms[i].instructionInfo->mnemonic);
            success = 0;
        } else {
            printf("\tSUCCESS instr %d:  \t\t%s, \t\texpected %s", i, instructionDisasm->instructionInfo->mnemonic, expected_instructionDisasms[i].instructionInfo->mnemonic);
        }

        /* Print the opcodes for debugging's sake */
        printf("\t\topcodes ");
        for (j = 0; j < instructionDisasm->instructionInfo->width; j++)
            printf("%02x ", instructionDisasm->opcode[j]);
        printf("\n");

        /* Compare disassembled operands */
        for (j = 0; j < 2; j++) {
           if (instructionDisasm->operandDisasms[j] != expected_instructionDisasms[i].operandDisasms[j]) {
                printf("\tFAILURE instr %d operand %d:\t0x%04x, \texpected 0x%04x\n", i, j, instructionDisasm->operandDisasms[j], expected_instructionDisasms[i].operandDisasms[j]);
                success = 0;
            } else {
                printf("\tSUCCESS instr %d operand %d:\t0x%04x, \texpected 0x%04x\n", i, j, instructionDisasm->operandDisasms[j], expected_instructionDisasms[i].operandDisasms[j]);
            }
        }
    }

    if (success) {
        printf("\tSUCCESS all checks passed!\n\n");
        return 0;
    } else {
        printf("\tFAILURE not all checks passed!\n\n");
    }

    return -1;
}

static struct picInstructionInfo *util_iset_lookup_by_mnemonic(int subarch, char *mnemonic) {
    int i;

    for (i = 0; i < PIC_TOTAL_INSTRUCTIONS[subarch]; i++) {
        if (strcmp(mnemonic, PIC_Instruction_Sets[subarch][i].mnemonic) == 0)
            return &PIC_Instruction_Sets[subarch][i];
    }

    return NULL;
}

/******************************************************************************/
/* PIC Disasm Stream Unit Tests */
/******************************************************************************/

int test_disasm_pic_unit_tests(void) {
    int i;
    int numTests = 0, passedTests = 0;
    struct picInstructionInfo *(*lookup)(int, char *) = util_iset_lookup_by_mnemonic;

    /* Check Sample Baseline Program */
    /* clrw; clrf 0x15; incf 5, f; movf 0x15, W; bsf 0x15, 3; btfsc 0x15, 2;
     * andlw 0xfe; call 0x50; goto 0x54; sleep; clrwdt; end */
    {
        uint8_t d[] = {0x40, 0x00, 0x75, 0x00, 0xa5, 0x02, 0x15, 0x02, 0x75, 0x05, 0x55, 0x06, 0xfe, 0x0e, 0x50, 0x09, 0x54, 0x0a, 0x03, 0x00, 0x04, 0x00, };
        uint32_t a[sizeof(d)]; for (i = 0; i < sizeof(d); i++) a[i] = i;
        struct picInstructionDisasm dis[] = {
                                                {0x00, {0}, lookup(PIC_SUBARCH_BASELINE, "clrw"), {0}},
                                                {0x02, {0}, lookup(PIC_SUBARCH_BASELINE, "clrf"), {0x15}},
                                                {0x04, {0}, lookup(PIC_SUBARCH_BASELINE, "incf"), {0x5, 0x1}},
                                                {0x06, {0}, lookup(PIC_SUBARCH_BASELINE, "movf"), {0x15, 0x0}},
                                                {0x08, {0}, lookup(PIC_SUBARCH_BASELINE, "bsf"), {0x15, 0x3}},
                                                {0x0a, {0}, lookup(PIC_SUBARCH_BASELINE, "btfsc"), {0x15, 0x2}},
                                                {0x0c, {0}, lookup(PIC_SUBARCH_BASELINE, "andlw"), {0xfe}},
                                                {0x0e, {0}, lookup(PIC_SUBARCH_BASELINE, "call"), {0x50}},
                                                {0x10, {0}, lookup(PIC_SUBARCH_BASELINE, "goto"), {0x54}},
                                                {0x12, {0}, lookup(PIC_SUBARCH_BASELINE, "sleep"), {0}},
                                                {0x14, {0}, lookup(PIC_SUBARCH_BASELINE, "clrwdt"), {0}},
                                            };
        if (test_disasm_pic_unit_test_run("Sample Program Baseline", PIC_SUBARCH_BASELINE, &d[0], &a[0], sizeof(d), &dis[0], sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    /* Check Sample Midrange Program */
    /* clrw; clrf 0x15; incf 5, f; movf 0x15, W; bsf 0x15, 3; btfsc 0x15, 2;
     * andlw 0xfe; call 0x600; goto 0x604; sleep; clrwdt; end */
    {
        uint8_t d[] = {0x03, 0x01, 0x95, 0x01, 0x85, 0x0a, 0x15, 0x08, 0x95, 0x15, 0x15, 0x19, 0xfe, 0x39, 0x00, 0x26, 0x04, 0x2e, 0x63, 0x00, 0x64, 0x00, };
        uint32_t a[sizeof(d)]; for (i = 0; i < sizeof(d); i++) a[i] = i;
        struct picInstructionDisasm dis[] = {
                                                {0x00, {0}, lookup(PIC_SUBARCH_MIDRANGE, "clrw"), {0}},
                                                {0x02, {0}, lookup(PIC_SUBARCH_MIDRANGE, "clrf"), {0x15}},
                                                {0x04, {0}, lookup(PIC_SUBARCH_MIDRANGE, "incf"), {0x5, 0x1}},
                                                {0x06, {0}, lookup(PIC_SUBARCH_MIDRANGE, "movf"), {0x15, 0x0}},
                                                {0x08, {0}, lookup(PIC_SUBARCH_MIDRANGE, "bsf"), {0x15, 0x3}},
                                                {0x0a, {0}, lookup(PIC_SUBARCH_MIDRANGE, "btfsc"), {0x15, 0x2}},
                                                {0x0c, {0}, lookup(PIC_SUBARCH_MIDRANGE, "andlw"), {0xfe}},
                                                {0x0e, {0}, lookup(PIC_SUBARCH_MIDRANGE, "call"), {0x600}},
                                                {0x10, {0}, lookup(PIC_SUBARCH_MIDRANGE, "goto"), {0x604}},
                                                {0x12, {0}, lookup(PIC_SUBARCH_MIDRANGE, "sleep"), {0}},
                                                {0x14, {0}, lookup(PIC_SUBARCH_MIDRANGE, "clrwdt"), {0}},
                                            };
        if (test_disasm_pic_unit_test_run("Sample Program Midrange", PIC_SUBARCH_MIDRANGE, &d[0], &a[0], sizeof(d), &dis[0], sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    /* Check Sample Midrange Enhanced Program */
    /* clrw; clrf 0x15; incf 5, f; movf 0x15, W; bsf 0x15, 3; btfsc 0x15, 2;
     * a: andlw 0xfe; call 0x600; goto 0x604; sleep; clrwdt;
     * lslf 0x15, f; addwfc 0x15, W; decfsz 0x15, f; movlp 0x7f; bra a; brw;
     * reset; addfsr FSR0, 0x0a; moviw ++FSR1; moviw --FSR1; moviw FSR0++;
     * moviw FSR0--; moviw 5[FSR1]; end */
    {
        uint8_t d[] = {0x03, 0x01, 0x95, 0x01, 0x85, 0x0a, 0x15, 0x08, 0x95, 0x15, 0x15, 0x19, 0xfe, 0x39, 0x00, 0x26, 0x04, 0x2e, 0x63, 0x00, 0x64, 0x00, 0x95, 0x35, 0x15, 0x3d, 0x95, 0x0b, 0xff, 0x31, 0xf6, 0x33, 0x0b, 0x00, 0x01, 0x00, 0x0a, 0x31, 0x14, 0x00, 0x15, 0x00, 0x12, 0x00, 0x13, 0x00, 0x45, 0x3f};
        uint32_t a[sizeof(d)]; for (i = 0; i < sizeof(d); i++) a[i] = i;
        struct picInstructionDisasm dis[] = {
                                                {0x00, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "clrw"), {0}},
                                                {0x02, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "clrf"), {0x15}},
                                                {0x04, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "incf"), {0x5, 0x1}},
                                                {0x06, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "movf"), {0x15, 0x0}},
                                                {0x08, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "bsf"), {0x15, 0x3}},
                                                {0x0a, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "btfsc"), {0x15, 0x2}},
                                                {0x0c, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "andlw"), {0xfe}},
                                                {0x0e, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "call"), {0x600}},
                                                {0x10, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "goto"), {0x604}},
                                                {0x12, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "sleep"), {0}},
                                                {0x14, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "clrwdt"), {0}},
                                                {0x16, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "lslf"), {0x15, 0x1}},
                                                {0x18, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "addwfc"), {0x15}},
                                                {0x1a, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "decfsz"), {0x15, 0x1}},
                                                {0x1c, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "movlp"), {0x7f}},
                                                {0x1e, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "bra"), {-0x14}},
                                                {0x20, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "brw"), {0}},
                                                {0x22, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "reset"), {0}},
                                                {0x24, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "addfsr"), {0x0, 0x0a}},
                                                {0x26, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "moviw"), {0x1, 0x0}},
                                                {0x28, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "moviw"), {0x1, 0x1}},
                                                {0x2a, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "moviw"), {0x0, 0x2}},
                                                {0x2c, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "moviw"), {0x0, 0x3}},
                                                {0x2e, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "moviw")+1, {0x1, 0x5}},
                                            };
        if (test_disasm_pic_unit_test_run("Sample Program Midrange Enhanced", PIC_SUBARCH_MIDRANGE_ENHANCED, &d[0], &a[0], sizeof(d), &dis[0], sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;

    }

    /* Check EOF lone byte */
    /* Lone byte due to EOF */
    {
        uint8_t d[] = {0x18};
        uint32_t a[] = {0x500};
        struct picInstructionDisasm dis[] = {
                                                {0x500, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "db"), {0x18}},
                                            };
        if (test_disasm_pic_unit_test_run("EOF Lone Byte", PIC_SUBARCH_MIDRANGE_ENHANCED, &d[0], &a[0], sizeof(d), &dis[0], sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    /* Check boundary lone byte */
    /* Lone byte due to address change */
    {
        uint8_t d[] = {0x18, 0x12, 0x33};
        uint32_t a[] = {0x500, 0x502, 0x503};
        struct picInstructionDisasm dis[] = {
                                                {0x500, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "db"), {0x18}},
                                                {0x502, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "bra"), {-0x1DC}},
                                            };
        if (test_disasm_pic_unit_test_run("Boundary Lone Byte", PIC_SUBARCH_MIDRANGE_ENHANCED, &d[0], &a[0], sizeof(d), &dis[0], sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    printf("%d / %d tests passed.\n\n", passedTests, numTests);

    if (passedTests == numTests)
        return 0;

    return -1;
}

