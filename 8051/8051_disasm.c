#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <bytestream.h>
#include <disasmstream.h>
#include <instruction.h>

#include "8051_instruction_set.h"
#include "8051_support.h"

/******************************************************************************/
/* 8051 Instruction/Directive Accessor Functions */
/******************************************************************************/

extern uint32_t a8051_instruction_get_address(struct instruction *instr);
extern unsigned int a8051_instruction_get_width(struct instruction *instr);
extern unsigned int a8051_instruction_get_num_operands(struct instruction *instr);
extern unsigned int a8051_instruction_get_opcodes(struct instruction *instr, uint8_t *dest);
extern int a8051_instruction_get_str_address_label(struct instruction *instr, char *dest, int size, int flags);
extern int a8051_instruction_get_str_address(struct instruction *instr, char *dest, int size, int flags);
extern int a8051_instruction_get_str_opcodes(struct instruction *instr, char *dest, int size, int flags);
extern int a8051_instruction_get_str_mnemonic(struct instruction *instr, char *dest, int size, int flags);
extern int a8051_instruction_get_str_operand(struct instruction *instr, char *dest, int size, int index, int flags);
extern int a8051_instruction_get_str_comment(struct instruction *instr, char *dest, int size, int flags);
extern void a8051_instruction_free(struct instruction *instr);

extern unsigned int a8051_directive_get_num_operands(struct instruction *instr);
extern int a8051_directive_get_str_mnemonic(struct instruction *instr, char *dest, int size, int flags);
extern int a8051_directive_get_str_operand(struct instruction *instr, char *dest, int size, int index, int flags);
extern void a8051_directive_free(struct instruction *instr);

/******************************************************************************/
/* 8051 Disassembly Stream Support */
/******************************************************************************/

struct disasmstream_8051_state {
    /* 3-byte opcode buffer */
    uint8_t data[3];
    uint32_t address[3];
    unsigned int len;

    /* initialized, eof encountered, end directive, invalid instruction
     * booleans */
    int initialized, eof, end_directive, invalid_instruction;
    /* Next expected address */
    uint32_t next_address;
};

int disasmstream_8051_init(struct DisasmStream *self) {
    /* Allocate stream state */
    self->state = malloc(sizeof(struct disasmstream_8051_state));
    if (self->state == NULL) {
        self->error = "Error allocating disasm stream state!";
        return STREAM_ERROR_ALLOC;
    }
    /* Initialize stream state */
    memset(self->state, 0, sizeof(struct disasmstream_8051_state));

    /* Reset the error to NULL */
    self->error = NULL;

    /* Initialize the input stream */
    if (self->in->stream_init(self->in) < 0) {
        self->error = "Error in input stream initialization!";
        return STREAM_ERROR_INPUT;
    }

    return 0;
}

int disasmstream_8051_close(struct DisasmStream *self) {
    /* Free stream state memory */
    free(self->state);

    /* Close input stream */
    if (self->in->stream_close(self->in) < 0) {
        self->error = "Error in input stream close!";
        return STREAM_ERROR_INPUT;
    }

    return 0;
}

/******************************************************************************/
/* Core of the 8051 Disassembler */
/******************************************************************************/

static int util_disasm_directive(struct instruction *instr, char *name, uint32_t value);
static int util_disasm_instruction(struct instruction *instr, struct a8051InstructionInfo *instructionInfo, struct disasmstream_8051_state *state);
static void util_disasm_operands(struct a8051InstructionDisasm *instructionDisasm);
static void util_opbuffer_shift(struct disasmstream_8051_state *state, int n);
static int util_opbuffer_len_consecutive(struct disasmstream_8051_state *state);
static struct a8051InstructionInfo *util_iset_lookup_by_opcode(uint8_t opcode);

int disasmstream_8051_read(struct DisasmStream *self, struct instruction *instr) {
    struct disasmstream_8051_state *state = (struct disasmstream_8051_state *)self->state;
    int decodeAttempts, lenConsecutive;

    /* Clear the destination instruction structure */
    memset(instr, 0, sizeof(struct instruction));

    for (decodeAttempts = 0; decodeAttempts < 5; decodeAttempts++) {
        /* Count the number of consective bytes in our opcode buffer */
        lenConsecutive = util_opbuffer_len_consecutive(state);

        /* If we decoded all bytes, reached EOF, returned an end directive,
         * then return EOF too */
        if (lenConsecutive == 0 && state->len == 0 && state->eof && state->end_directive)
            return STREAM_EOF;

        /* If we decoded all bytes and reached EOF, then return an end
         * directive */
        if (lenConsecutive == 0 && state->len == 0 && state->eof) {
            /* Emit an end directive */
            if (util_disasm_directive(instr, A8051_DIRECTIVE_NAME_END, 0) < 0) {
                self->error = "Error allocating memory for directive!";
                return STREAM_ERROR_FAILURE;
            }
            state->end_directive = 1;
            return 0;
        }

        /* If the address jumped since the last instruction or we're
         * uninitialized, then return an org directive */
        if (lenConsecutive > 0 && (state->address[0] != state->next_address || !state->initialized)) {
            /* Emit an origin directive */
            if (util_disasm_directive(instr, A8051_DIRECTIVE_NAME_ORIGIN, state->address[0]) < 0) {
                self->error = "Error allocating memory for directive!";
                return STREAM_ERROR_FAILURE;
            }
            /* Update our state's next expected address */
            state->next_address = state->address[0];
            state->initialized = 1;
            return 0;
        }

        if (lenConsecutive > 0) {
            struct a8051InstructionInfo *instructionInfo;

            if ((instructionInfo = util_iset_lookup_by_opcode(state->data[0])) == NULL) {
                /* This should never happen because the 8051 instruction set
                 * spans 0x00 - 0xFF */
                self->error = "Error, catastrophic failure! Malformed instruction set!";
                return STREAM_ERROR_FAILURE;
            }

            /* If a longer instruction was cut off by an address or EOF
             * boundary, return raw .DB bytes until the consecutive bytes are
             * depleted */
            if (state->invalid_instruction || (lenConsecutive < instructionInfo->width && (state->len > lenConsecutive || state->eof))) {
                /* Disassembly a raw .DB byte "instruction" */
                if (util_disasm_instruction(instr, &A8051_Instruction_Set[A8051_ISET_INDEX_BYTE], state) < 0) {
                    self->error = "Error allocating memory for disassembled instruction!";
                    return STREAM_ERROR_FAILURE;
                }
                /* If we disassembled our last byte before the boundary, turn
                 * off the invalid_instruction flag */
                if (lenConsecutive - 1 == 0)
                    state->invalid_instruction = 0;
                else
                    state->invalid_instruction = 1;
                return 0;

            /* If we've colleted enough bytes to decode this instruction */
            } else if (lenConsecutive == instructionInfo->width) {
                /* Disassemble and return the instruction */
                if (util_disasm_instruction(instr, instructionInfo, state) < 0) {
                    self->error = "Error allocating memory for disassembled instruction!";
                    return STREAM_ERROR_FAILURE;
                }
                return 0;

            }
        }

        /* Otherwise, read another byte into our opcode buffer below */

        uint8_t readData;
        uint32_t readAddr;
        int ret;

        /* Read the next data byte from the opcode stream */
        ret = self->in->stream_read(self->in, &readData, &readAddr);
        if (ret == STREAM_EOF) {
            /* Record encountered EOF */
            state->eof = 1;
        } else if (ret < 0) {
            self->error = "Error in opcode stream read!";
            return STREAM_ERROR_INPUT;
        }

        if (ret == 0) {
            /* If we have an opcode buffer overflow (this should never happen
             * if the decoding logic above is correct) */
            if (state->len == sizeof(state->data)) {
                self->error = "Error, catastrophic failure! Opcode buffer overflowed!";
                return STREAM_ERROR_FAILURE;
            }

            /* Append the data / address to our opcode buffer */
            state->data[state->len] = readData;
            state->address[state->len] = readAddr;
            state->len++;
        }
    }

    /* We should have returned an instruction above */
    self->error = "Error, catastrophic failure! No decoding logic invoked!";
    return STREAM_ERROR_FAILURE;

    return 0;
}

static int util_disasm_directive(struct instruction *instr, char *name, uint32_t value) {
    struct a8051Directive *directive;

    /* Allocate directive structure */
    directive = malloc(sizeof(struct a8051Directive));
    if (directive == NULL)
        return -1;

    /* Clear the structure */
    memset(directive, 0, sizeof(struct a8051Directive));

    /* Load name and value */
    directive->name = name;
    directive->value = value;

    /* Setup the instruction structure */
    instr->data = directive;
    instr->type = DISASM_TYPE_DIRECTIVE;
    instr->get_num_operands = a8051_directive_get_num_operands;
    instr->get_str_mnemonic = a8051_directive_get_str_mnemonic;
    instr->get_str_operand = a8051_directive_get_str_operand;
    instr->free = a8051_directive_free;

    return 0;
}

static int util_disasm_instruction(struct instruction *instr, struct a8051InstructionInfo *instructionInfo, struct disasmstream_8051_state *state) {
    struct a8051InstructionDisasm *instructionDisasm;
    int i;

    /* Allocate disassembled instruction structure */
    instructionDisasm = malloc(sizeof(struct a8051InstructionDisasm));
    if (instructionDisasm == NULL)
        return -1;

    /* Clear the structure */
    memset(instructionDisasm, 0, sizeof(struct a8051InstructionDisasm));

    /* Load instruction info, address, opcodes, and operands */
    instructionDisasm->instructionInfo = instructionInfo;
    instructionDisasm->address = state->address[0];
    for (i = 0; i < instructionInfo->width; i++)
        instructionDisasm->opcode[i] = state->data[i];
    util_disasm_operands(instructionDisasm);
    util_opbuffer_shift(state, instructionInfo->width);

    /* Setup the instruction structure */
    instr->data = instructionDisasm;
    instr->type = DISASM_TYPE_INSTRUCTION;
    instr->get_address = a8051_instruction_get_address;
    instr->get_width = a8051_instruction_get_width;
    instr->get_num_operands = a8051_instruction_get_num_operands;
    instr->get_opcodes = a8051_instruction_get_opcodes;
    instr->get_str_address_label = a8051_instruction_get_str_address_label;
    instr->get_str_address = a8051_instruction_get_str_address;
    instr->get_str_opcodes = a8051_instruction_get_str_opcodes;
    instr->get_str_mnemonic = a8051_instruction_get_str_mnemonic;
    instr->get_str_operand = a8051_instruction_get_str_operand;
    instr->get_str_comment = a8051_instruction_get_str_comment;
    instr->free = a8051_instruction_free;

    /* Update our state's next expected address */
    state->next_address = instructionDisasm->address + instructionDisasm->instructionInfo->width;

    return 0;
}

static void util_disasm_operands(struct a8051InstructionDisasm *instructionDisasm) {
    struct a8051InstructionInfo *instructionInfo = instructionDisasm->instructionInfo;
    int i, encodedIndex;

    /* Index of encoded operands into opcode array */
    encodedIndex = 1;

    /* Disassemble the operands */
    for (i = 0; i < instructionInfo->numOperands; i++) {
        switch (instructionInfo->operandTypes[i]) {
            case OPERAND_R:
                instructionDisasm->operandDisasms[i] = instructionDisasm->opcode[0] & 0x07;
                break;
            case OPERAND_IND_R:
                instructionDisasm->operandDisasms[i] = instructionDisasm->opcode[0] & 0x01;
                break;

            /* Source / Destination direct address stored in reverse order from
             * mnemonic operands */
            case OPERAND_ADDR_DIRECT_SRC:
                instructionDisasm->operandDisasms[i] = instructionDisasm->opcode[1];
                break;
            case OPERAND_ADDR_DIRECT_DST:
                instructionDisasm->operandDisasms[i] = instructionDisasm->opcode[2];
                break;

            case OPERAND_ADDR_DIRECT:
            case OPERAND_ADDR_BIT:
            case OPERAND_ADDR_NOT_BIT:
            case OPERAND_IMMED:
                if (instructionInfo == &A8051_Instruction_Set[A8051_ISET_INDEX_BYTE]) {
                    instructionDisasm->operandDisasms[i] = instructionDisasm->opcode[0];
                } else {
                    instructionDisasm->operandDisasms[i] = (instructionDisasm->opcode[encodedIndex]);
                    encodedIndex += 1;
                }
                break;
            case OPERAND_IMMED_16:
            case OPERAND_ADDR_16:
                instructionDisasm->operandDisasms[i] = ((instructionDisasm->opcode[encodedIndex]) << 8) | instructionDisasm->opcode[encodedIndex+1];
                encodedIndex += 2;
                break;
            case OPERAND_ADDR_11:
                instructionDisasm->operandDisasms[i] = ((instructionDisasm->opcode[0] & 0xE0) << 3) | instructionDisasm->opcode[1];
                break;
            case OPERAND_ADDR_RELATIVE:
                /* Relative branch address is 8 bits, two's complement form */

                /* If the sign bit is set */
                if (instructionDisasm->opcode[encodedIndex] & (1 << 7)) {
                    /* Manually sign-extend to the 32-bit container */
                    instructionDisasm->operandDisasms[i] = (int32_t) ( ( ~(instructionDisasm->opcode[encodedIndex]) + 1 ) & 0xff );
                    instructionDisasm->operandDisasms[i] = -instructionDisasm->operandDisasms[i];
                } else {
                    instructionDisasm->operandDisasms[i] = (int32_t) ( (instructionDisasm->opcode[encodedIndex]) & 0xff );
                }
                encodedIndex += 1;

                break;
            /* Other implied operands are fully specified by their operand type */
            case OPERAND_A:
            case OPERAND_AB:
            case OPERAND_C:
            case OPERAND_DPTR:
            case OPERAND_IND_DPTR:
            case OPERAND_IND_A_DPTR:
            case OPERAND_IND_A_PC:
            default:
                instructionDisasm->operandDisasms[i] = 0;
                break;
        }
    }
}

static void util_opbuffer_shift(struct disasmstream_8051_state *state, int n) {
    int i, j;

    for (i = 0; i < n; i++) {
        /* Shift the data and address slots down by one */
        for (j = 0; j < sizeof(state->data) - 1; j++) {
            state->data[j] = state->data[j+1];
            state->address[j] = state->address[j+1];
        }
        state->data[j] = 0x00;
        state->address[j] = 0x00;
        /* Update the opcode buffer length */
        if (state->len > 0)
            state->len--;
    }
}

static int util_opbuffer_len_consecutive(struct disasmstream_8051_state *state) {
    int i, lenConsecutive;

    lenConsecutive = 0;
    for (i = 0; i < state->len; i++) {
        /* If there is a greater than 1 byte gap between addresses */
        if (i > 0 && (state->address[i] - state->address[i-1]) != 1)
            break;
        lenConsecutive++;
    }

    return lenConsecutive;
}

static struct a8051InstructionInfo *util_iset_lookup_by_opcode(uint8_t opcode) {
    return &A8051_Instruction_Set[opcode];
}

