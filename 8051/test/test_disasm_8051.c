#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <bytestream.h>
#include <disasmstream.h>
#include <file/debug.h>
#include <instruction.h>

#include <8051/8051_support.h>
#include <8051/8051_instruction_set.h>

/******************************************************************************/
/* 8051 Disasm Stream Test Instrumentation */
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

    /* Setup the 8051 Disasm Stream */
    ds.in = &bs;
    ds.stream_init = disasmstream_8051_init;
    ds.stream_close = disasmstream_8051_close;
    ds.stream_read = disasmstream_8051_read;

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

static int test_disasm_8051_unit_test_run(char *name, uint8_t *test_data, uint32_t *test_address, unsigned int test_len, struct a8051InstructionDisasm *expected_instructionDisasms, unsigned int expected_len) {
    struct a8051InstructionDisasm *instructionDisasm;
    struct instruction instrs[96];
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

        instructionDisasm = (struct a8051InstructionDisasm *)instrs[i].data;

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

static struct a8051InstructionInfo *util_iset_lookup_by_mnemonic(char *mnemonic) {
    int i;

    for (i = 0; i < A8051_TOTAL_INSTRUCTIONS; i++) {
        if (strcmp(mnemonic, A8051_Instruction_Set[i].mnemonic) == 0)
            return &A8051_Instruction_Set[i];
    }

    printf("Error! Could not find instruction \"%s\"!\n", mnemonic);
    exit(-1);

    return &A8051_Instruction_Set[A8051_ISET_INDEX_BYTE];
}

static struct a8051InstructionInfo *util_iset_lookup_by_opcode(uint8_t opcode) {
    return &A8051_Instruction_Set[opcode];
}


/******************************************************************************/
/* 8051 Disasm Stream Unit Tests */
/******************************************************************************/

int test_disasm_8051_unit_tests(void) {
    int numTests = 0, passedTests = 0;
    int i;
    struct a8051InstructionInfo *(*lookup)(char *) = util_iset_lookup_by_mnemonic;
    struct a8051InstructionInfo *(*lookup_opcode)(uint8_t) = util_iset_lookup_by_opcode;

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
     * label2: end */
    {
        uint8_t d[] = {0x00, 0x04, 0x15, 0x23, 0x06, 0x17, 0x0d, 0x24, 0xaa, 0x35, 0x23, 0x46, 0x57, 0x6d, 0x03, 0x13, 0x14, 0xf4, 0x74, 0xaa, 0x75, 0x23, 0xbb, 0x76, 0xcc, 0x77, 0xdd, 0x79, 0xee, 0x85, 0x25, 0x23, 0x89, 0x23, 0x87, 0x23, 0xa0, 0x22, 0xa1, 0x01, 0x12, 0x05, 0x01, 0x40, 0xda, 0x60, 0xd8, 0xda, 0xd6, 0xb4, 0x55, 0xd3, 0xb5, 0x23, 0xd0, 0xb6, 0x55, 0xcd, 0xb7, 0x55, 0xca, 0xbc, 0x55, 0xc7, 0xc0, 0x23, 0xd0, 0x20, 0x22, 0x32, 0x73, 0xa3, 0x84, 0x82, 0x43, 0xf0, 0xe0, 0x93, 0x83, 0x73};
        uint32_t a[sizeof(d)]; for (i = 0; i < sizeof(d); i++) a[i] = i;
        struct a8051InstructionDisasm dis[] = {
                                                {0x00, {0}, lookup("nop"), {0}},
                                                {0x01, {0}, lookup("inc"), {0}},
                                                {0x02, {0}, lookup("dec")+1, {0x23}},
                                                {0x04, {0}, lookup("inc")+2, {0x00}},
                                                {0x05, {0}, lookup("dec")+3, {0x01}},
                                                {0x06, {0}, lookup("inc")+9, {0x05}},
                                                {0x07, {0}, lookup("add"), {0, 0xaa}},
                                                {0x09, {0}, lookup("addc")+1, {0, 0x23}},
                                                {0x0b, {0}, lookup("orl")+4, {0, 0x00}},
                                                {0x0c, {0}, lookup("anl")+5, {0, 0x01}},
                                                {0x0d, {0}, lookup("xrl")+11, {0, 0x05}},
                                                {0x0e, {0}, lookup("rr"), {0}},
                                                {0x0f, {0}, lookup("rrc"), {0}},
                                                {0x10, {0}, lookup("dec"), {0}},
                                                {0x11, {0}, lookup_opcode(0xf4), {0}},
                                                {0x12, {0}, lookup("mov"), {0, 0xaa}},
                                                {0x14, {0}, lookup("mov")+1, {0x23, 0xbb}},
                                                {0x17, {0}, lookup("mov")+2, {0x00, 0xcc}},
                                                {0x19, {0}, lookup("mov")+3, {0x01, 0xdd}},
                                                {0x1b, {0}, lookup("mov")+5, {0x01, 0xee}},
                                                {0x1d, {0}, lookup_opcode(0x85), {0x23, 0x25}},
                                                {0x20, {0}, lookup_opcode(0x89), {0x23, 0x01}},
                                                {0x22, {0}, lookup_opcode(0x87), {0x23, 0x01}},
                                                {0x24, {0}, lookup_opcode(0xa0), {0, 0x22}},
                                                {0x26, {0}, lookup_opcode(0xa1), {0x501}},
                                                {0x28, {0}, lookup("lcall"), {0x501}},
                                                {0x2b, {0}, lookup("jc"), {-0x26}},
                                                {0x2d, {0}, lookup("jz"), {-0x28}},
                                                {0x2f, {0}, lookup("djnz")+5, {0x02, -0x2a}},
                                                {0x31, {0}, lookup("cjne"), {0, 0x55, 0}},
                                                {0x34, {0}, lookup("cjne")+1, {0 ,0x23, 0}},
                                                {0x37, {0}, lookup("cjne")+2, {0x00, 0x55, 0}},
                                                {0x3a, {0}, lookup("cjne")+3, {0x01, 0x55, 0}},
                                                {0x3d, {0}, lookup("cjne")+8, {0x04, 0x55, 0}},
                                                {0x40, {0}, lookup("push"), {0x23}},
                                                {0x42, {0}, lookup("pop"), {0x20}},
                                                {0x44, {0}, lookup("ret"), {0}},
                                                {0x45, {0}, lookup("reti"), {0}},
                                                {0x46, {0}, lookup("jmp"), {0}},
                                                {0x47, {0}, lookup_opcode(0xa3), {0}},
                                                {0x48, {0}, lookup("div"), {0}},
                                                {0x49, {0}, lookup_opcode(0x82), {0, 0x43}},
                                                {0x4b, {0}, lookup_opcode(0xf0), {0}},
                                                {0x4c, {0}, lookup("movx"), {0}},
                                                {0x4d, {0}, lookup_opcode(0x93), {0}},
                                                {0x4e, {0}, lookup("movc"), {0}},
                                                {0x4f, {0}, lookup("jmp"), {0}},
                                            };
        if (test_disasm_8051_unit_test_run("8051 Sample Program", (uint8_t *)d, (uint32_t *)a, sizeof(d), (struct a8051InstructionDisasm *)dis, sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    /* Check instruction cut off by EOF */
    /* mov A, #32h cut off by EOF */
    {
        uint8_t d[] = {0x74};
        uint32_t a[sizeof(d)]; for (i = 0; i < sizeof(d); i++) a[i] = i;
        struct a8051InstructionDisasm dis[] = {
                                                {0x00, {0}, lookup(".db"), {0x74}},
                                            };
        if (test_disasm_8051_unit_test_run("8051 Instruction EOF Cutoff", (uint8_t *)d, (uint32_t *)a, sizeof(d), (struct a8051InstructionDisasm *)dis, sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    /* Check instruction cut off by address boundary */
    /* orl 45h, #aah cut off */
    {
        uint8_t d[] = {0x43, 0x45, 0xaa};
        uint32_t a[] = {0x100, 0x101, 0x500};
        struct a8051InstructionDisasm dis[] = {
                                                {0x100, {0}, lookup(".db"), {0x43}},
                                                {0x101, {0}, lookup(".db"), {0x45}},
                                                {0x500, {0}, lookup(".db"), {0xaa}},
                                            };
        if (test_disasm_8051_unit_test_run("8051 Instruction Address Boundary Cutoff", (uint8_t *)d, (uint32_t *)a, sizeof(d), (struct a8051InstructionDisasm *)dis, sizeof(dis)/sizeof(dis[0])) == 0)
            passedTests++;
        numTests++;
    }

    printf("%d / %d tests passed.\n\n", passedTests, numTests);

    if (passedTests == numTests)
        return 0;

    return -1;
}

