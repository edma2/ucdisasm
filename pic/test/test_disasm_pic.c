#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
    } else if (subarch == PIC_SUBARCH_PIC18) {
        ds.stream_init = disasmstream_pic_pic18_init;
        ds.stream_close = disasmstream_pic_pic18_close;
        ds.stream_read = disasmstream_pic_pic18_read;
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

    printf("\tds.stream_read() read %d instructions\n", *output_len);

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
    struct instruction instrs[48];
    unsigned int len, instrLen;
    int ret, i, ei, j;
    int success;

    printf("Running test \"%s\"\n", name);

    /* Run the Disasm Stream on the test vectors */
    ret = test_disasmstream(subarch, test_data, test_address, test_len, (struct instruction *)&instrs, &len);
    if (ret != 0) {
        printf("\tFAILURE ret != 0\n\n");
        return -1;
    }
    printf("\tSUCCESS ret == 0\n");

    /* Count the number of actual instructions */
    for (instrLen = 0, i = 0; i < len; i++) {
        if (instrs[i].type == DISASM_TYPE_INSTRUCTION)
            instrLen++;
    }

    /* Compare number of disassembled instructions */
    if (instrLen != expected_len) {
        printf("\tFAILURE len (%d) != expected_len (%d)\n\n", instrLen, expected_len);
        return -1;
    }
    printf("\tSUCCESS len (%d) == expected_len (%d)\n", instrLen, expected_len);


    success = 1;

    /* Compare each disassembled instruction */
    for (i = 0, ei = 0; i < len; i++) {
        /* Disregard non-instruction types */
        if (instrs[i].type != DISASM_TYPE_INSTRUCTION) {
            instrs[i].free(&instrs[i]);
            continue;
        }

        instructionDisasm = (struct picInstructionDisasm *)instrs[i].data;

        printf("\n");

        /* Compare instruction address */
        if (instructionDisasm->address != expected_instructionDisasms[ei].address) {
            printf("\tFAILURE instr %d address:\t0x%04x, \texpected 0x%04x\n", ei, instructionDisasm->address, expected_instructionDisasms[ei].address);
            success = 0;
        } else {
            printf("\tSUCCESS instr %d address:\t0x%04x, \texpected 0x%04x\n", ei, instructionDisasm->address, expected_instructionDisasms[ei].address);
        }

        /* Compare instruction identified */
        if (instructionDisasm->instructionInfo != expected_instructionDisasms[ei].instructionInfo) {
            printf("\tFAILURE instr %d:  \t\t%s, \t\texpected %s", ei, instructionDisasm->instructionInfo->mnemonic, expected_instructionDisasms[ei].instructionInfo->mnemonic);
            success = 0;
        } else {
            printf("\tSUCCESS instr %d:  \t\t%s, \t\texpected %s", ei, instructionDisasm->instructionInfo->mnemonic, expected_instructionDisasms[ei].instructionInfo->mnemonic);
        }

        /* Print the opcodes for debugging's sake */
        printf("\t\topcodes ");
        for (j = 0; j < instructionDisasm->instructionInfo->width; j++)
            printf("%02x ", instructionDisasm->opcode[j]);
        printf("\n");

        /* Compare disassembled operands */
        for (j = 0; j < 2; j++) {
           if (instructionDisasm->operandDisasms[j] != expected_instructionDisasms[ei].operandDisasms[j]) {
                printf("\tFAILURE instr %d operand %d:\t0x%04x, \texpected 0x%04x\n", ei, j, instructionDisasm->operandDisasms[j], expected_instructionDisasms[ei].operandDisasms[j]);
                success = 0;
            } else {
                printf("\tSUCCESS instr %d operand %d:\t0x%04x, \texpected 0x%04x\n", ei, j, instructionDisasm->operandDisasms[j], expected_instructionDisasms[ei].operandDisasms[j]);
            }
        }

        /* Free instruction */
        instrs[i].free(&instrs[i]);

        ei++;
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

    printf("Error! Could not find instruction \"%s\"!\n", mnemonic);
    exit(-1);

    return &PIC_Instruction_Sets[subarch][PIC_ISET_INDEX_WORD(subarch)];
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
                                                {0x0e, {0}, lookup(PIC_SUBARCH_BASELINE, "call"), {0xa0}},
                                                {0x10, {0}, lookup(PIC_SUBARCH_BASELINE, "goto"), {0xa8}},
                                                {0x12, {0}, lookup(PIC_SUBARCH_BASELINE, "sleep"), {0}},
                                                {0x14, {0}, lookup(PIC_SUBARCH_BASELINE, "clrwdt"), {0}},
                                            };
        if (test_disasm_pic_unit_test_run("PIC Baseline Sample Program", PIC_SUBARCH_BASELINE, (uint8_t *)d, (uint32_t *)a, sizeof(d), (struct picInstructionDisasm *)dis, sizeof(dis)/sizeof(dis[0])) == 0)
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
                                                {0x0e, {0}, lookup(PIC_SUBARCH_MIDRANGE, "call"), {0xc00}},
                                                {0x10, {0}, lookup(PIC_SUBARCH_MIDRANGE, "goto"), {0xc08}},
                                                {0x12, {0}, lookup(PIC_SUBARCH_MIDRANGE, "sleep"), {0}},
                                                {0x14, {0}, lookup(PIC_SUBARCH_MIDRANGE, "clrwdt"), {0}},
                                            };
        if (test_disasm_pic_unit_test_run("PIC Midrange Sample Program", PIC_SUBARCH_MIDRANGE, (uint8_t *)d, (uint32_t *)a, sizeof(d), (struct picInstructionDisasm *)dis, sizeof(dis)/sizeof(dis[0])) == 0)
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
                                                {0x0e, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "call"), {0xc00}},
                                                {0x10, {0}, lookup(PIC_SUBARCH_MIDRANGE_ENHANCED, "goto"), {0xc08}},
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
        if (test_disasm_pic_unit_test_run("PIC Midrange Enhanced Sample Program", PIC_SUBARCH_MIDRANGE_ENHANCED, (uint8_t *)d, (uint32_t *)a, sizeof(d), (struct picInstructionDisasm *)dis, sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;

    }

    /* Check Sample PIC18 Program */
    /* a:  clrf 0x31, 1; movwf 0x20, 0; cpfsgt 0x35, 1; decf 0x15, 0, 0;
     * addwf 0x15, 0, 1; incfsz 0x15, 1, 0; movf 0x15, 1, 1;
     * movff 0x123, 0x256; bcf 0x15, 3, 1; btfsc 0x15, 2, 0; btg 0x15, 1, 1;
     * b:  bc b; bnov c; bra a; c:  clrwdt; daw; sleep; nop; call a, 1;
     * call a, 0; goto c; retfie 1; retlw 0x42; addlw 0x23; mullw 0x32;
     * lfsr 2, 0xabc; tblrd*; tblrd*+; tblrd*-; tblrd+*; tblwt*; tblwt*+;
     * tblwt*-; tblwt+*; end */
    {
        uint8_t d[] = {0x31, 0x6b, 0x20, 0x6e, 0x35, 0x65, 0x15, 0x04, 0x15, 0x25, 0x15, 0x3e, 0x15, 0x53, 0x23, 0xc1, 0x56, 0xf2, 0x15, 0x97, 0x15, 0xb4, 0x15, 0x73, 0xff, 0xe2, 0x01, 0xe5, 0xf1, 0xd7, 0x04, 0x00, 0x07, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0xed, 0x00, 0xf0, 0x00, 0xec, 0x00, 0xf0, 0x0f, 0xef, 0x00, 0xf0, 0x11, 0x00, 0x42, 0x0c, 0x23, 0x0f, 0x32, 0x0d, 0x2a, 0xee, 0xbc, 0xf0, 0x08, 0x00, 0x09, 0x00, 0x0a, 0x00, 0x0b, 0x00, 0x0c, 0x00, 0x0d, 0x00, 0x0e, 0x00, 0x0f, 0x00,};
        uint32_t a[sizeof(d)]; for (i = 0; i < sizeof(d); i++) a[i] = i;
        struct picInstructionDisasm dis[] = {
                                                {0x00, {0}, lookup(PIC_SUBARCH_PIC18, "clrf"), {0x31, 0x1}},
                                                {0x02, {0}, lookup(PIC_SUBARCH_PIC18, "movwf"), {0x20, 0x0}},
                                                {0x04, {0}, lookup(PIC_SUBARCH_PIC18, "cpfsgt"), {0x35, 0x1}},
                                                {0x06, {0}, lookup(PIC_SUBARCH_PIC18, "decf"), {0x15, 0x0, 0x0}},
                                                {0x08, {0}, lookup(PIC_SUBARCH_PIC18, "addwf"), {0x15, 0x0, 0x1}},
                                                {0x0a, {0}, lookup(PIC_SUBARCH_PIC18, "incfsz"), {0x15, 0x1, 0x0}},
                                                {0x0c, {0}, lookup(PIC_SUBARCH_PIC18, "movf"), {0x15, 0x1, 0x1}},
                                                {0x0e, {0}, lookup(PIC_SUBARCH_PIC18, "movff"), {0x123, 0x256}},
                                                {0x12, {0}, lookup(PIC_SUBARCH_PIC18, "bcf"), {0x15, 0x3, 0x1}},
                                                {0x14, {0}, lookup(PIC_SUBARCH_PIC18, "btfsc"), {0x15, 0x2, 0x0}},
                                                {0x16, {0}, lookup(PIC_SUBARCH_PIC18, "btg"), {0x15, 0x1, 0x1}},
                                                {0x18, {0}, lookup(PIC_SUBARCH_PIC18, "bc"), {-0x2}},
                                                {0x1a, {0}, lookup(PIC_SUBARCH_PIC18, "bnov"), {0x2}},
                                                {0x1c, {0}, lookup(PIC_SUBARCH_PIC18, "bra"), {-0x1e}},
                                                {0x1e, {0}, lookup(PIC_SUBARCH_PIC18, "clrwdt"), {0}},
                                                {0x20, {0}, lookup(PIC_SUBARCH_PIC18, "daw"), {0}},
                                                {0x22, {0}, lookup(PIC_SUBARCH_PIC18, "sleep"), {0}},
                                                {0x24, {0}, lookup(PIC_SUBARCH_PIC18, "nop"), {0}},
                                                {0x26, {0}, lookup(PIC_SUBARCH_PIC18, "call"), {0x0, 0x1}},
                                                {0x2a, {0}, lookup(PIC_SUBARCH_PIC18, "call"), {0x0, 0x0}},
                                                {0x2e, {0}, lookup(PIC_SUBARCH_PIC18, "goto"), {0x1e}},
                                                {0x32, {0}, lookup(PIC_SUBARCH_PIC18, "retfie"), {0x1}},
                                                {0x34, {0}, lookup(PIC_SUBARCH_PIC18, "retlw"), {0x42}},
                                                {0x36, {0}, lookup(PIC_SUBARCH_PIC18, "addlw"), {0x23}},
                                                {0x38, {0}, lookup(PIC_SUBARCH_PIC18, "mullw"), {0x32}},
                                                {0x3a, {0}, lookup(PIC_SUBARCH_PIC18, "lfsr"), {0x2, 0xabc}},
                                                {0x3e, {0}, lookup(PIC_SUBARCH_PIC18, "tblrd*"), {0}},
                                                {0x40, {0}, lookup(PIC_SUBARCH_PIC18, "tblrd*+"), {0}},
                                                {0x42, {0}, lookup(PIC_SUBARCH_PIC18, "tblrd*-"), {0}},
                                                {0x44, {0}, lookup(PIC_SUBARCH_PIC18, "tblrd+*"), {0}},
                                                {0x46, {0}, lookup(PIC_SUBARCH_PIC18, "tblwt*"), {0}},
                                                {0x48, {0}, lookup(PIC_SUBARCH_PIC18, "tblwt*+"), {0}},
                                                {0x4a, {0}, lookup(PIC_SUBARCH_PIC18, "tblwt*-"), {0}},
                                                {0x4c, {0}, lookup(PIC_SUBARCH_PIC18, "tblwt+*"), {0}},
                                            };
        if (test_disasm_pic_unit_test_run("PIC PIC18 Sample Program", PIC_SUBARCH_PIC18, (uint8_t *)d, (uint32_t *)a, sizeof(d), (struct picInstructionDisasm *)dis, sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    /* Check 32-bit instructions */
    /* a: movff 0x123, 0x256; c: call 0x0, 1; goto c; lfsr 2, 0xabc; end */
    {
        uint8_t d[] = {0x23, 0xc1, 0x56, 0xf2, 0x00, 0xed, 0x00, 0xf0, 0x02, 0xef, 0x00, 0xf0, 0x2a, 0xee, 0xbc, 0xf0};
        uint32_t a[sizeof(d)]; for (i = 0; i < sizeof(d); i++) a[i] = i;
        struct picInstructionDisasm dis[] = {
                                                {0x00, {0}, lookup(PIC_SUBARCH_PIC18, "movff"), {0x123, 0x256}},
                                                {0x04, {0}, lookup(PIC_SUBARCH_PIC18, "call"), {0x0, 0x1}},
                                                {0x08, {0}, lookup(PIC_SUBARCH_PIC18, "goto"), {0x4}},
                                                {0x0c, {0}, lookup(PIC_SUBARCH_PIC18, "lfsr"), {0x2, 0xabc}},
                                            };
        if (test_disasm_pic_unit_test_run("PIC PIC18 32-bit Instructions", PIC_SUBARCH_PIC18, (uint8_t *)d, (uint32_t *)a, sizeof(d), (struct picInstructionDisasm *)dis, sizeof(dis)/sizeof(dis[0])) == 0)
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
        if (test_disasm_pic_unit_test_run("PIC Midrange Enhanced EOF Lone Byte", PIC_SUBARCH_MIDRANGE_ENHANCED, (uint8_t *)d, (uint32_t *)a, sizeof(d), (struct picInstructionDisasm *)dis, sizeof(dis)/sizeof(dis[0])) == 0)
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
        if (test_disasm_pic_unit_test_run("PIC Midrange Enhanced Boundary Lone Byte", PIC_SUBARCH_MIDRANGE_ENHANCED, (uint8_t *)d, (uint32_t *)a, sizeof(d), (struct picInstructionDisasm *)dis, sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    /* Check EOF lone 32-bit instruction */
    /* "call 0x500, 1" instruction cut short by EOF */
    {
        uint8_t d[] = {0x80, 0xed, 0x02, 0xf0, 0x80, 0xed, 0x02};
        uint32_t a[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
        struct picInstructionDisasm dis[] = {
                                                {0x00, {0}, lookup(PIC_SUBARCH_PIC18, "call"), {0x500, 0x1}},
                                                {0x04, {0}, lookup(PIC_SUBARCH_PIC18, "dw"), {0xed80}},
                                                {0x06, {0}, lookup(PIC_SUBARCH_PIC18, "db"), {0x02}},
                                            };
        if (test_disasm_pic_unit_test_run("PIC PIC18 EOF Lone 32-bit Instruction", PIC_SUBARCH_PIC18, (uint8_t *)d, (uint32_t *)a, sizeof(d), (struct picInstructionDisasm *)dis, sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    /* Check boundary lone 32-bit instruction */
    /* "call 0x500, 1" instruction cut short by address change */
    {
        uint8_t d[] = {0x80, 0xed, 0x02, 0xf0, 0x80, 0xed, 0x02, 0xf0};
        uint32_t a[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x20, 0x21};
        struct picInstructionDisasm dis[] = {
                                                {0x00, {0}, lookup(PIC_SUBARCH_PIC18, "call"), {0x500, 0x1}},
                                                {0x04, {0}, lookup(PIC_SUBARCH_PIC18, "dw"), {0xed80}},
                                                {0x20, {0}, lookup(PIC_SUBARCH_PIC18, "nop")+1, {0}},
                                            };
        if (test_disasm_pic_unit_test_run("PIC PIC18 Boundary Lone 32-bit Instruction", PIC_SUBARCH_PIC18, (uint8_t *)d, (uint32_t *)a, sizeof(d), (struct picInstructionDisasm *)dis, sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    printf("%d / %d tests passed.\n\n", passedTests, numTests);

    if (passedTests == numTests)
        return 0;

    return -1;
}

