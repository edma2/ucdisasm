#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <bytestream.h>
#include <disasmstream.h>
#include <file/debug.h>
#include <instruction.h>

#include <avr/avr_support.h>
#include <avr/avr_instruction_set.h>

/******************************************************************************/
/* AVR Disasm Stream Test Instrumentation */
/******************************************************************************/

static int test_disasmstream(uint8_t *test_data, uint32_t *test_address, unsigned int test_len, struct instruction *output_instrs, unsigned int *output_len) {
    struct ByteStream bs;
    struct DisasmStream ds;
    int ret;

    /* Setup a Debug Byte Stream */
    bs.in = NULL;
    bs.stream_init = bytestream_debug_init;
    bs.stream_close = bytestream_debug_close;
    bs.stream_read = bytestream_debug_read;

    /* Setup the AVR Disasm Stream */
    ds.in = &bs;
    ds.stream_init = disasmstream_avr_init;
    ds.stream_close = disasmstream_avr_close;
    ds.stream_read = disasmstream_avr_read;

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

    for (; ret != STREAM_EOF; output_instrs++) {
        /* Disassemble an instruction */
        ret = ds.stream_read(&ds, output_instrs);
        printf("\tds.stream_read(): %d\n", ret);
        if (ret == STREAM_EOF) {
            printf("\t\tEOF encountered\n");
        } else if (ret == 0) {
            *output_len = *output_len + 1;
        } else {
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

static int test_disasm_avr_unit_test_run(char *name, uint8_t *test_data, uint32_t *test_address, unsigned int test_len, struct avrInstructionDisasm *expected_instructionDisasms, unsigned int expected_len) {
    struct avrInstructionDisasm *instructionDisasm;
    struct instruction instrs[16];
    unsigned int len;
    int ret, i, j;
    int success;

    printf("Running test \"%s\"\n", name);

    /* Run the Disasm Stream on the test vectors */
    ret = test_disasmstream(test_data, test_address, test_len, &instrs[0], &len);
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
        instructionDisasm = (struct avrInstructionDisasm *)instrs[i].instructionDisasm;

        /* Compare instruction address */
        if (instructionDisasm->address != expected_instructionDisasms[i].address) {
            printf("\tFAILURE instr %d address:\t0x%04x, \texpected 0x%04x\n", i, instructionDisasm->address, expected_instructionDisasms[i].address);
            success = 0;
        } else {
            printf("\tSUCCESS instr %d address:\t0x%04x, \texpected 0x%04x\n", i, instructionDisasm->address, expected_instructionDisasms[i].address);
        }

        /* Compare instruction identified */
        if (instructionDisasm->instructionInfo != expected_instructionDisasms[i].instructionInfo) {
            printf("\tFAILURE instr %d:  \t\t%s, \t\texpected %s\n", i, instructionDisasm->instructionInfo->mnemonic, expected_instructionDisasms[i].instructionInfo->mnemonic);
            success = 0;
        } else {
            printf("\tSUCCESS instr %d:  \t\t%s, \t\texpected %s\n", i, instructionDisasm->instructionInfo->mnemonic, expected_instructionDisasms[i].instructionInfo->mnemonic);
        }

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

static struct avrInstructionInfo *util_iset_lookup_by_mnemonic(char *mnemonic) {
    int i;

    for (i = 0; i < AVR_TOTAL_INSTRUCTIONS; i++) {
        if (strcmp(mnemonic, AVR_Instruction_Set[i].mnemonic) == 0)
            return &AVR_Instruction_Set[i];
    }

    return NULL;
}

/******************************************************************************/
/* AVR Disasm Stream Unit Tests */
/******************************************************************************/

int test_disasm_avr_unit_tests(void) {
    int numTests = 0, passedTests = 0;
    struct avrInstructionInfo *(*lookup)(char *) = util_iset_lookup_by_mnemonic;

    /* Check Sample Program */
    /* rjmp .0; ser R16; out $17, R16; out $18, R16; dec R16; rjmp .-6 */
    {
        uint8_t d[] =  {0x00, 0xc0, 0x0f, 0xef, 0x07, 0xbb, 0x08, 0xbb, 0x0a, 0x95, 0xfd, 0xcf};
        uint32_t a[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b};
        struct avrInstructionDisasm dis[] = {
                                                {0x00, {0}, lookup("rjmp"), {0, 0x00}},
                                                {0x02, {0}, lookup("ser"), {16, 0x00}},
                                                {0x04, {0}, lookup("out"), {0x17, 0x10}},
                                                {0x06, {0}, lookup("out"), {0x18, 0x10}},
                                                {0x08, {0}, lookup("dec"), {16, 0x00}},
                                                {0x0a, {0}, lookup("rjmp"), {-6, 0x00}},
                                            };
        if (test_disasm_avr_unit_test_run("Sample Program", &d[0], &a[0], sizeof(d), &dis[0], sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    /* Check 32-bit Instructions */
    /* jmp 0x2abab4; call 0x1f00e; sts 0x1234, r2; lds r3, 0x6780 */
    {
        uint8_t d[] =  {0xad, 0x94, 0x5a, 0x5d, 0x0e, 0x94, 0x07, 0xf8, 0x20, 0x92, 0x34, 0x12, 0x30, 0x90, 0x80, 0x67};
        uint32_t a[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
        struct avrInstructionDisasm dis[] = {
                                                {0x00, {0}, lookup("jmp"), {0x2abab4, 0x00}},
                                                {0x04, {0}, lookup("call"), {0x1f00e, 0x00}},
                                                {0x08, {0}, lookup("sts"), {0x2468, 2}},
                                                {0x0c, {0}, lookup("lds"), {3, 0xcf00}},
                                            };
        if (test_disasm_avr_unit_test_run("32-bit Instructions", &d[0], &a[0], sizeof(d), &dis[0], sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    /* Check EOF lone byte */
    /* Lone byte due to EOF */
    {
        uint8_t d[] = {0x18};
        uint32_t a[] = {0x500};
        struct avrInstructionDisasm dis[] = {
                                                {0x500, {0}, lookup(".db"), {0x18, 0x00}},
                                            };
        if (test_disasm_avr_unit_test_run("EOF Lone Byte", &d[0], &a[0], sizeof(d), &dis[0], sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    /* Check boundary lone byte */
    /* Lone byte due to address change */
    {
        uint8_t d[] = {0x18, 0x12, 0x33};
        uint32_t a[] = {0x500, 0x502, 0x503};
        struct avrInstructionDisasm dis[] = {
                                                {0x500, {0}, lookup(".db"), {0x18, 0x00}},
                                                {0x502, {0}, lookup("cpi"), {0x11, 0x32}},
                                            };
        if (test_disasm_avr_unit_test_run("Boundary Lone Byte", &d[0], &a[0], sizeof(d), &dis[0], sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    /* Check EOF lone wide instruction */
    /* Call instruction 0x94ae 0xab XX cut short by EOF */
    {
        uint8_t d[] = {0xae, 0x94, 0xab};
        uint32_t a[] = {0x500, 0x501, 0x502};
        struct avrInstructionDisasm dis[] = {   {0x500, {0}, lookup(".dw"), {0x94ae, 0x00}},
                                                {0x502, {0}, lookup(".db"), {0xab, 0x00}},
                                            };
        if (test_disasm_avr_unit_test_run("EOF Lone Wide Instruction", &d[0], &a[0], sizeof(d), &dis[0], sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    /* Check boundary lone wide instruction */
    /* Call instruction 500: 0x94ae | 504: 0xab 0xcd cut short by address change */
    {
        uint8_t d[] = {0xae, 0x94, 0xab, 0xcd};
        uint32_t a[] = {0x500, 0x501, 0x504, 0x505};
        struct avrInstructionDisasm dis[] = {   {0x500, {0}, lookup(".dw"), {0x94ae, 0x00}},
                                                {0x504, {0}, lookup("rjmp"), {-0x4aa, 0x00}},
                                            };
        if (test_disasm_avr_unit_test_run("Boundary Lone Wide Instruction", &d[0], &a[0], sizeof(d), &dis[0], sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    printf("%d / %d tests passed.\n\n", passedTests, numTests);

    if (passedTests == numTests)
        return 0;

    return -1;
}

