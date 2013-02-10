#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

static int test_disasm_avr_unit_test_run(char *name, uint8_t *test_data, uint32_t *test_address, unsigned int test_len, struct avrInstructionDisasm *expected_instructionDisasms, unsigned int expected_len) {
    struct avrInstructionDisasm *instructionDisasm;
    struct instruction instrs[16];
    unsigned int len, instrLen;
    int ret, i, ei, j;
    int success;

    printf("Running test \"%s\"\n", name);

    /* Run the Disasm Stream on the test vectors */
    ret = test_disasmstream(test_data, test_address, test_len, (struct instruction *)&instrs, &len);
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

        instructionDisasm = (struct avrInstructionDisasm *)instrs[i].data;

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

static struct avrInstructionInfo *util_iset_lookup_by_mnemonic(char *mnemonic) {
    int i;

    for (i = 0; i < AVR_TOTAL_INSTRUCTIONS; i++) {
        if (strcmp(mnemonic, AVR_Instruction_Set[i].mnemonic) == 0)
            return &AVR_Instruction_Set[i];
    }

    printf("Error! Could not find instruction \"%s\"!\n", mnemonic);
    exit(-1);

    return &AVR_Instruction_Set[AVR_ISET_INDEX_WORD];
}

/******************************************************************************/
/* AVR Disasm Stream Unit Tests */
/******************************************************************************/

int test_disasm_avr_unit_tests(void) {
    int numTests = 0, passedTests = 0;
    int i;
    struct avrInstructionInfo *(*lookup)(char *) = util_iset_lookup_by_mnemonic;

    /* Check Sample Program */
    /* rjmp .0; ser R16; out $17, R16; out $18, R16; dec R16; rjmp .-6 */
    {
        uint8_t d[] =  {0x00, 0xc0, 0x0f, 0xef, 0x07, 0xbb, 0x08, 0xbb, 0x0a, 0x95, 0xfd, 0xcf};
        uint32_t a[sizeof(d)]; for (i = 0; i < sizeof(d); i++) a[i] = i;
        struct avrInstructionDisasm dis[] = {
                                                {0x00, {0}, lookup("rjmp"), {0}},
                                                {0x02, {0}, lookup("ser"), {16}},
                                                {0x04, {0}, lookup("out"), {0x17, 0x10}},
                                                {0x06, {0}, lookup("out"), {0x18, 0x10}},
                                                {0x08, {0}, lookup("dec"), {16}},
                                                {0x0a, {0}, lookup("rjmp"), {-6}},
                                            };
        if (test_disasm_avr_unit_test_run("AVR8 Sample Program", (uint8_t *)d, (uint32_t *)a, sizeof(d), (struct avrInstructionDisasm *)dis, sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    /* Check 32-bit Instructions */
    /* jmp 0x2abab4; call 0x1f00e; sts 0x1234, r2; lds r3, 0x6780 */
    {
        uint8_t d[] =  {0xad, 0x94, 0x5a, 0x5d, 0x0e, 0x94, 0x07, 0xf8, 0x20, 0x92, 0x34, 0x12, 0x30, 0x90, 0x80, 0x67};
        uint32_t a[sizeof(d)]; for (i = 0; i < sizeof(d); i++) a[i] = i;
        struct avrInstructionDisasm dis[] = {
                                                {0x00, {0}, lookup("jmp"), {0x2abab4}},
                                                {0x04, {0}, lookup("call"), {0x1f00e}},
                                                {0x08, {0}, lookup("sts"), {0x2468, 2}},
                                                {0x0c, {0}, lookup("lds"), {3, 0xcf00}},
                                            };
        if (test_disasm_avr_unit_test_run("AVR8 32-bit Instructions", (uint8_t *)d, (uint32_t *)a, sizeof(d), (struct avrInstructionDisasm *)dis, sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    /* Check EOF lone byte */
    /* Lone byte due to EOF */
    {
        uint8_t d[] = {0x18};
        uint32_t a[sizeof(d)]; for (i = 0; i < sizeof(d); i++) a[i] = i;
        struct avrInstructionDisasm dis[] = {
                                                {0x00, {0}, lookup(".db"), {0x18}},
                                            };
        if (test_disasm_avr_unit_test_run("AVR8 EOF Lone Byte", (uint8_t *)d, (uint32_t *)a, sizeof(d), (struct avrInstructionDisasm *)dis, sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    /* Check boundary lone byte */
    /* Lone byte due to address change */
    {
        uint8_t d[] = {0x18, 0x12, 0x33};
        uint32_t a[] = {0x100, 0x502, 0x503};
        struct avrInstructionDisasm dis[] = {
                                                {0x100, {0}, lookup(".db"), {0x18}},
                                                {0x502, {0}, lookup("cpi"), {0x11, 0x32}},
                                            };
        if (test_disasm_avr_unit_test_run("AVR8 Boundary Lone Byte", (uint8_t *)d, (uint32_t *)a, sizeof(d), (struct avrInstructionDisasm *)dis, sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    /* Check EOF lone wide instruction */
    /* Call instruction 0x94ae 0xab XX cut short by EOF */
    {
        uint8_t d[] = {0xae, 0x94, 0xab};
        uint32_t a[] = {0x500, 0x501, 0x502};
        struct avrInstructionDisasm dis[] = {
                                                {0x500, {0}, lookup(".dw"), {0x94ae}},
                                                {0x502, {0}, lookup(".db"), {0xab}},
                                            };
        if (test_disasm_avr_unit_test_run("AVR8 EOF Lone Wide Instruction", (uint8_t *)d, (uint32_t *)a, sizeof(d), (struct avrInstructionDisasm *)dis, sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    /* Check boundary lone wide instruction */
    /* Call instruction 500: 0x94ae | 504: 0xab 0xcd cut short by address change */
    {
        uint8_t d[] = {0xae, 0x94, 0xab, 0xcd};
        uint32_t a[] = {0x100, 0x101, 0x504, 0x505};
        struct avrInstructionDisasm dis[] = {
                                                {0x100, {0}, lookup(".dw"), {0x94ae}},
                                                {0x504, {0}, lookup("rjmp"), {-0x4aa}},
                                            };
        if (test_disasm_avr_unit_test_run("AVR8 Boundary Lone Wide Instruction", (uint8_t *)d, (uint32_t *)a, sizeof(d), (struct avrInstructionDisasm *)dis, sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    printf("%d / %d tests passed.\n\n", passedTests, numTests);

    if (passedTests == numTests)
        return 0;

    return -1;
}

