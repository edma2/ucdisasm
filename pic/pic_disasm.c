#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <bytestream.h>
#include <disasmstream.h>
#include <instruction.h>

#include "pic_instruction_set.h"
#include "pic_support.h"

/******************************************************************************/
/* PIC Instruction/Directive Accessor Functions */
/******************************************************************************/

extern uint32_t pic_instruction_get_address(struct instruction *instr);
extern unsigned int pic_instruction_get_width(struct instruction *instr);
extern unsigned int pic_instruction_get_num_operands(struct instruction *instr);
extern unsigned int pic_instruction_get_opcodes(struct instruction *instr, uint8_t *dest);
extern int pic_instruction_get_str_address_label(struct instruction *instr, char *dest, int size, int flags);
extern int pic_instruction_get_str_address(struct instruction *instr, char *dest, int size, int flags);
extern int pic_instruction_get_str_opcodes(struct instruction *instr, char *dest, int size, int flags);
extern int pic_instruction_get_str_mnemonic(struct instruction *instr, char *dest, int size, int flags);
extern int pic_instruction_get_str_operand(struct instruction *instr, char *dest, int size, int index, int flags);
extern int pic_instruction_get_str_comment(struct instruction *instr, char *dest, int size, int flags);
extern void pic_instruction_free(struct instruction *instr);

extern unsigned int pic_directive_get_num_operands(struct instruction *instr);
extern int pic_directive_get_str_mnemonic(struct instruction *instr, char *dest, int size, int flags);
extern int pic_directive_get_str_operand(struct instruction *instr, char *dest, int size, int index, int flags);
extern void pic_directive_free(struct instruction *instr);

/******************************************************************************/
/* PIC Baseline / Midrange / Midrange Enhanced Disassembly Stream Support */
/******************************************************************************/

struct disasmstream_pic_state {
    /* Architecture */
    int subarch;
    /* 4-byte opcode buffer */
    uint8_t data[4];
    uint32_t address[4];
    unsigned int len;

    /* Initialized, eof encountered, end directive booleans */
    int initialized, eof, end_directive;
    /* Next expected address */
    uint32_t next_address;
};

static int disasmstream_pic_init(struct DisasmStream *self, int subarch) {
    /* Allocate stream state */
    self->state = malloc(sizeof(struct disasmstream_pic_state));
    if (self->state == NULL) {
        self->error = "Error allocating disasm stream state!";
        return STREAM_ERROR_ALLOC;
    }
    /* Initialize stream state */
    memset(self->state, 0, sizeof(struct disasmstream_pic_state));
    ((struct disasmstream_pic_state *)self->state)->subarch = subarch;

    /* Reset the error to NULL */
    self->error = NULL;

    /* Initialize the input stream */
    if (self->in->stream_init(self->in) < 0) {
        self->error = "Error in input stream initialization!";
        return STREAM_ERROR_INPUT;
    }

    return 0;
}

/* Wrapper init functions for different sub-architectures */
int disasmstream_pic_baseline_init(struct DisasmStream *self) { return disasmstream_pic_init(self, PIC_SUBARCH_BASELINE); }
int disasmstream_pic_midrange_init(struct DisasmStream *self) { return disasmstream_pic_init(self, PIC_SUBARCH_MIDRANGE); }
int disasmstream_pic_midrange_enhanced_init(struct DisasmStream *self) { return disasmstream_pic_init(self, PIC_SUBARCH_MIDRANGE_ENHANCED); }
int disasmstream_pic_pic18_init(struct DisasmStream *self) { return disasmstream_pic_init(self, PIC_SUBARCH_PIC18); }

int disasmstream_pic_close(struct DisasmStream *self) {
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
/* Core of the PIC Disassembler */
/******************************************************************************/

static int util_disasm_directive(struct instruction *instr, char *name, uint32_t value);
static int util_disasm_instruction(struct instruction *instr, struct picInstructionInfo *instructionInfo, struct disasmstream_pic_state *state);
static void util_disasm_operands(struct picInstructionDisasm *instructionDisasm);
static int32_t util_disasm_operand(struct picInstructionInfo *instruction, uint32_t operand, int index);
static void util_opbuffer_shift(struct disasmstream_pic_state *state, int n);
static int util_opbuffer_len_consecutive(struct disasmstream_pic_state *state);
static struct picInstructionInfo *util_iset_lookup_by_opcode(int subarch, uint16_t opcode);
static int util_bits_data_from_mask(uint16_t data, uint16_t mask);

int disasmstream_pic_read(struct DisasmStream *self, struct instruction *instr) {
    struct disasmstream_pic_state *state = (struct disasmstream_pic_state *)self->state;
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

        /* If we decoded all bytes, reached EOF, then return an end directive */
        if (lenConsecutive == 0 && state->len == 0 && state->eof) {
            /* Emit an end directive */
            if (util_disasm_directive(instr, PIC_DIRECTIVE_NAME_END, 0) < 0) {
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
            if (util_disasm_directive(instr, PIC_DIRECTIVE_NAME_ORIGIN, state->address[0]) < 0) {
                self->error = "Error allocating memory for directive!";
                return STREAM_ERROR_FAILURE;
            }
            /* Update our state's next expected address */
            state->next_address = state->address[0];
            state->initialized = 1;
            return 0;
        }

        /* Edge case: when input stream changes address or reaches EOF with 1
         * undecoded byte */
        if (lenConsecutive == 1 && (state->len > 1 || state->eof)) {
            /* Disassemble a raw .DB byte "instruction" */
            if (util_disasm_instruction(instr, &PIC_Instruction_Sets[state->subarch][PIC_ISET_INDEX_BYTE(state->subarch)], state) < 0) {
                self->error = "Error allocating memory for disassembled instruction!";
                return STREAM_ERROR_FAILURE;
            }
            return 0;
        }

        /* Two or more consecutive bytes */
        if (lenConsecutive >= 2) {
            struct picInstructionInfo *instructionInfo;
            uint16_t opcode;

            /* Assemble the 16-bit opcode from little-endian input */
            opcode = (uint16_t)(state->data[1] << 8) | (uint16_t)(state->data[0]);
            /* Look up the instruction in our instruction set */
            if ( (instructionInfo = util_iset_lookup_by_opcode(state->subarch, opcode)) == NULL) {
                /* This should never happen because of the .DW instruction that
                 * matches any 16-bit opcode */
                self->error = "Error, catastrophic failure! Malformed instruction set!";
                return STREAM_ERROR_FAILURE;
            }

            /* If this is a 16-bit wide instruction */
            if (instructionInfo->width == 2) {
                /* Disassemble and return the 16-bit instruction */
                if (util_disasm_instruction(instr, instructionInfo, state) < 0) {
                    self->error = "Error allocating memory for disassembled instruction!";
                    return STREAM_ERROR_FAILURE;
                }
                return 0;

            /* Else, this is a 32-bit wide instruction */
            } else {
                /* We have read the complete 32-bit instruction */
                if (lenConsecutive == 4) {
                    /* Decode a 32-bit instruction */
                    if (util_disasm_instruction(instr, instructionInfo, state) < 0) {
                        self->error = "Error allocating memory for disassembled instruction!";
                        return STREAM_ERROR_FAILURE;
                    }
                    return 0;

                /* Edge case: when input stream changes address or reaches EOF
                 * with 3 or 2 undecoded long instruction bytes */
                } else if ((lenConsecutive == 3 && (state->len > 3 || state->eof)) ||
                           (lenConsecutive == 2 && (state->len > 2 || state->eof))) {
                    /* Return a raw .DW word "instruction" */
                    if (util_disasm_instruction(instr, &PIC_Instruction_Sets[state->subarch][PIC_ISET_INDEX_WORD(state->subarch)], state) < 0) {
                        self->error = "Error allocating memory for disassembled instruction!";
                        return STREAM_ERROR_FAILURE;
                    }
                    return 0;
                }

                /* Otherwise, read another byte into our opcode buffer below */
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
    struct picDirective *directive;

    /* Allocate directive structure */
    directive = malloc(sizeof(struct picDirective));
    if (directive == NULL)
        return -1;

    /* Clear the structure */
    memset(directive, 0, sizeof(struct picDirective));

    /* Load name and value */
    directive->name = name;
    directive->value = value;

    /* Setup the instruction structure */
    instr->data = directive;
    instr->type = DISASM_TYPE_DIRECTIVE;
    instr->get_num_operands = pic_directive_get_num_operands;
    instr->get_str_mnemonic = pic_directive_get_str_mnemonic;
    instr->get_str_operand = pic_directive_get_str_operand;
    instr->free = pic_directive_free;

    return 0;
}

static int util_disasm_instruction(struct instruction *instr, struct picInstructionInfo *instructionInfo, struct disasmstream_pic_state *state) {
    struct picInstructionDisasm *instructionDisasm;
    int i;

    /* Allocate disassembled instruction structure */
    instructionDisasm = malloc(sizeof(struct picInstructionDisasm));
    if (instructionDisasm == NULL)
        return -1;

    /* Clear the structure */
    memset(instructionDisasm, 0, sizeof(struct picInstructionDisasm));

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
    instr->get_address = pic_instruction_get_address;
    instr->get_width = pic_instruction_get_width;
    instr->get_num_operands = pic_instruction_get_num_operands;
    instr->get_opcodes = pic_instruction_get_opcodes;
    instr->get_str_address_label = pic_instruction_get_str_address_label;
    instr->get_str_address = pic_instruction_get_str_address;
    instr->get_str_opcodes = pic_instruction_get_str_opcodes;
    instr->get_str_mnemonic = pic_instruction_get_str_mnemonic;
    instr->get_str_operand = pic_instruction_get_str_operand;
    instr->get_str_comment = pic_instruction_get_str_comment;
    instr->free = pic_instruction_free;

    /* Update our state's next expected address */
    state->next_address = instructionDisasm->address + instructionDisasm->instructionInfo->width;

    return 0;
}

static void util_disasm_operands(struct picInstructionDisasm *instructionDisasm) {
    struct picInstructionInfo *instructionInfo = instructionDisasm->instructionInfo;
    int i;
    uint16_t opcode;
    uint32_t operand;

    opcode = ((uint16_t)instructionDisasm->opcode[1] << 8) | ((uint16_t)instructionDisasm->opcode[0]);

    /* Disassemble the operands */
    for (i = 0; i < instructionInfo->numOperands; i++) {
        /* Extract the operand bits */
        operand = util_bits_data_from_mask(opcode, instructionInfo->operandMasks[i]);

        /* Append extra bits if it's a long operand */
        if (instructionInfo->operandTypes[i] == OPERAND_LONG_ABSOLUTE_PROG_ADDRESS ||
            instructionInfo->operandTypes[i] == OPERAND_LONG_ABSOLUTE_DATA_ADDRESS)
            operand = (((uint32_t)instructionDisasm->opcode[3] & 0x0f) << 16) | ((uint32_t)instructionDisasm->opcode[2] << 8) | (uint32_t)operand;
        else if (instructionInfo->operandTypes[i] == OPERAND_LONG_LFSR_LITERAL)
            operand = ((uint32_t)operand << 8) | ((uint32_t)instructionDisasm->opcode[2]);
        else if (instructionInfo->operandTypes[i] == OPERAND_LONG_MOVFF_DATA_ADDRESS)
            operand = (((uint32_t)instructionDisasm->opcode[3] & 0x0f) << 8) | (uint32_t)instructionDisasm->opcode[2];

        /* Disassemble the operand */
        instructionDisasm->operandDisasms[i] = util_disasm_operand(instructionInfo, operand, i);
    }
}

static int32_t util_disasm_operand(struct picInstructionInfo *instruction, uint32_t operand, int index) {
    int32_t operandDisasm;
    uint32_t msb;

    switch (instruction->operandTypes[index]) {
        case OPERAND_ABSOLUTE_DATA_ADDRESS:
        case OPERAND_LONG_ABSOLUTE_DATA_ADDRESS:
            /* This is already a data address */
            operandDisasm = operand;
            break;

        case OPERAND_ABSOLUTE_PROG_ADDRESS:
        case OPERAND_LONG_ABSOLUTE_PROG_ADDRESS:
            /* Multiply by two to point to a byte address */
            operandDisasm = operand * 2;
            break;

        case OPERAND_SIGNED_LITERAL:
        case OPERAND_RELATIVE_PROG_ADDRESS:
            /* We got lucky, because it turns out that in all of the masks for
             * relative jumps / signed literals, the bits occupy the lowest
             * positions continuously (no breaks in the bit string). */
            /* Calculate the most significant bit of this signed data */
            msb = (instruction->operandMasks[index] + 1) >> 1;

            /* If the sign bit is set */
            if (operand & msb) {
                /* Manually sign-extend to the 32-bit container */
                operandDisasm = (int32_t) ( ( ~operand + 1 ) & instruction->operandMasks[index] );
                operandDisasm = -operandDisasm;
            } else {
                operandDisasm = (int32_t) ( operand & instruction->operandMasks[index] );
            }

            /* If this is an program address */
            if (instruction->operandTypes[index] == OPERAND_RELATIVE_PROG_ADDRESS) {
                /* Multiply by two to point to a byte address */
                operandDisasm *= 2;
            }

            break;

        default:
            /* Copy the operand with no additional processing */
            operandDisasm = operand;
            break;
    }

    return operandDisasm;
}

static void util_opbuffer_shift(struct disasmstream_pic_state *state, int n) {
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

static int util_opbuffer_len_consecutive(struct disasmstream_pic_state *state) {
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

static struct picInstructionInfo *util_iset_lookup_by_opcode(int subarch, uint16_t opcode) {
    int i, j;

    uint16_t instructionBits;

    for (i = 0; i < PIC_TOTAL_INSTRUCTIONS[subarch]; i++) {
        instructionBits = opcode;

        /* Mask out the don't care pits */
        instructionBits &= ~(PIC_Instruction_Sets[subarch][i].dontcareMask);

        /* Mask out the operands from the opcode */
        for (j = 0; j < PIC_Instruction_Sets[subarch][i].numOperands; j++)
            instructionBits &= ~(PIC_Instruction_Sets[subarch][i].operandMasks[j]);

        /* Compare left over instruction bits with the instruction mask */
        if (instructionBits == PIC_Instruction_Sets[subarch][i].instructionMask)
            return &PIC_Instruction_Sets[subarch][i];
    }

    return NULL;
}

static int util_bits_data_from_mask(uint16_t data, uint16_t mask) {
    uint16_t result;
    int i, j;

    result = 0;

    /* Sweep through mask from bits 0 to 15 */
    for (i = 0, j = 0; i < 16; i++) {
        /* If mask bit is set */
        if (mask & (1 << i)) {
            /* If data bit is set */
            if (data & (1 << i))
                result |= (1 << j);
            j++;
        }
    }

    return result;
}

