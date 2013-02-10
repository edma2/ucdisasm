#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <instruction.h>
#include <printstream_file.h>

#include "avr_instruction_set.h"

/* AVRASM formats */
#define AVR_FORMAT_OP_REGISTER(fmt)         "R" fmt ""      /* mov R0, R2 */
#define AVR_FORMAT_OP_IO_REGISTER(fmt)      "$" fmt ""      /* out $39, R16 */
#define AVR_FORMAT_OP_DATA_HEX(fmt)         "0x" fmt ""     /* ldi R16, 0x3d */
#define AVR_FORMAT_OP_DATA_BIN(fmt)         "0b" fmt ""     /* ldi R16, 0b00111101 */
#define AVR_FORMAT_OP_DATA_DEC(fmt)         "" fmt ""       /* ldi R16, 61 */
#define AVR_FORMAT_OP_BIT(fmt)              "" fmt ""       /* cbi $12, 7 */
#define AVR_FORMAT_OP_ABSOLUTE_ADDRESS(fmt) "0x" fmt ""     /* call 0x1234 */
#define AVR_FORMAT_OP_RELATIVE_ADDRESS(fmt) "." fmt ""      /* rjmp .4 */
#define AVR_FORMAT_OP_ADDRESS_LABEL(fmt)    "A_" fmt ""     /* call A_0004 */
#define AVR_FORMAT_OP_DES_ROUND(fmt)        "0x" fmt ""     /* des 0x01 */
#define AVR_FORMAT_OP_RAW_WORD(fmt)         "0x" fmt ""     /* .dw 0xabcd */
#define AVR_FORMAT_OP_RAW_BYTE(fmt)         "0x" fmt ""     /* .db 0xab */
#define AVR_FORMAT_ADDRESS(fmt)             "" fmt ":"      /* 0004: */
#define AVR_FORMAT_ADDRESS_LABEL(fmt)       "A_" fmt ":"    /* A_0004: */

/* Address filed width, e.g. 4 -> 0x0004 */
#define AVR_ADDRESS_WIDTH               4

/* AVR Instruction Accessor Functions */
uint32_t avr_instruction_get_address(struct instruction *instr);
unsigned int avr_instruction_get_width(struct instruction *instr);
unsigned int avr_instruction_get_num_operands(struct instruction *instr);
unsigned int avr_instruction_get_opcodes(struct instruction *instr, uint8_t *dest);
int avr_instruction_get_str_address_label(struct instruction *instr, char *dest, int size, int flags);
int avr_instruction_get_str_address(struct instruction *instr, char *dest, int size, int flags);
int avr_instruction_get_str_opcodes(struct instruction *instr, char *dest, int size, int flags);
int avr_instruction_get_str_mnemonic(struct instruction *instr, char *dest, int size, int flags);
int avr_instruction_get_str_operand(struct instruction *instr, char *dest, int size, int index, int flags);
int avr_instruction_get_str_comment(struct instruction *instr, char *dest, int size, int flags);
void avr_instruction_free(struct instruction *instr);

/* AVR Directive Accessor Functions */
unsigned int avr_directive_get_num_operands(struct instruction *instr);
int avr_directive_get_str_mnemonic(struct instruction *instr, char *dest, int size, int flags);
int avr_directive_get_str_operand(struct instruction *instr, char *dest, int size, int index, int flags);
void avr_directive_free(struct instruction *instr);

/******************************************************************************/
/* AVR Instructions */
/******************************************************************************/

uint32_t avr_instruction_get_address(struct instruction *instr) {
    struct avrInstructionDisasm *instructionDisasm = (struct avrInstructionDisasm *)instr->data;
    return instructionDisasm->address;
}

unsigned int avr_instruction_get_width(struct instruction *instr) {
    struct avrInstructionDisasm *instructionDisasm = (struct avrInstructionDisasm *)instr->data;
    return instructionDisasm->instructionInfo->width;
}

unsigned int avr_instruction_get_num_operands(struct instruction *instr) {
    struct avrInstructionDisasm *instructionDisasm = (struct avrInstructionDisasm *)instr->data;
    return instructionDisasm->instructionInfo->numOperands;
}

unsigned int avr_instruction_get_opcodes(struct instruction *instr, uint8_t *dest) {
    struct avrInstructionDisasm *instructionDisasm = (struct avrInstructionDisasm *)instr->data;
    int i;

    for (i = 0; i < instructionDisasm->instructionInfo->width; i++)
        *dest++ = instructionDisasm->opcode[i];

    return instructionDisasm->instructionInfo->width;
}

int avr_instruction_get_str_address_label(struct instruction *instr, char *dest, int size, int flags) {
    struct avrInstructionDisasm *instructionDisasm = (struct avrInstructionDisasm *)instr->data;
    return snprintf(dest, size, AVR_FORMAT_ADDRESS_LABEL("%0*x"), AVR_ADDRESS_WIDTH, instructionDisasm->address);
}

int avr_instruction_get_str_address(struct instruction *instr, char *dest, int size, int flags) {
    struct avrInstructionDisasm *instructionDisasm = (struct avrInstructionDisasm *)instr->data;
    return snprintf(dest, size, AVR_FORMAT_ADDRESS("%*x"), AVR_ADDRESS_WIDTH, instructionDisasm->address);
}

int avr_instruction_get_str_opcodes(struct instruction *instr, char *dest, int size, int flags) {
    struct avrInstructionDisasm *instructionDisasm = (struct avrInstructionDisasm *)instr->data;

    if (instructionDisasm->instructionInfo->width == 1)
        return snprintf(dest, size, "%02x         ", instructionDisasm->opcode[0]);
    else if (instructionDisasm->instructionInfo->width == 2)
        return snprintf(dest, size, "%02x %02x      ", instructionDisasm->opcode[1], instructionDisasm->opcode[0]);
    else if (instructionDisasm->instructionInfo->width == 4)
        return snprintf(dest, size, "%02x %02x %02x %02x", instructionDisasm->opcode[3], instructionDisasm->opcode[2], instructionDisasm->opcode[1], instructionDisasm->opcode[0]);

    return 0;
}

int avr_instruction_get_str_mnemonic(struct instruction *instr, char *dest, int size, int flags) {
    struct avrInstructionDisasm *instructionDisasm = (struct avrInstructionDisasm *)instr->data;
    return snprintf(dest, size, "%s", instructionDisasm->instructionInfo->mnemonic);
}

int avr_instruction_get_str_operand(struct instruction *instr, char *dest, int size, int index, int flags) {
    struct avrInstructionDisasm *instructionDisasm = (struct avrInstructionDisasm *)instr->data;

    if (index < 0 || index > instructionDisasm->instructionInfo->numOperands - 1)
        return 0;

    switch (instructionDisasm->instructionInfo->operandTypes[index]) {
        case OPERAND_REGISTER:
        case OPERAND_REGISTER_STARTR16:
        case OPERAND_REGISTER_EVEN_PAIR:
        case OPERAND_REGISTER_EVEN_PAIR_STARTR24:
            return snprintf(dest, size, AVR_FORMAT_OP_REGISTER("%d"), instructionDisasm->operandDisasms[index]);
        case OPERAND_IO_REGISTER:
            return snprintf(dest, size, AVR_FORMAT_OP_IO_REGISTER("%02x"), instructionDisasm->operandDisasms[index]);
        case OPERAND_BIT:
            return snprintf(dest, size, AVR_FORMAT_OP_BIT("%d"), instructionDisasm->operandDisasms[index]);
        case OPERAND_DES_ROUND:
            return snprintf(dest, size, AVR_FORMAT_OP_DES_ROUND("%d"), instructionDisasm->operandDisasms[index]);
        case OPERAND_RAW_WORD:
            return snprintf(dest, size, AVR_FORMAT_OP_RAW_WORD("%04x"), instructionDisasm->operandDisasms[index]);
        case OPERAND_RAW_BYTE:
            return snprintf(dest, size, AVR_FORMAT_OP_RAW_BYTE("%02x"), instructionDisasm->operandDisasms[index]);
        case OPERAND_X:
            return snprintf(dest, size, "X");
        case OPERAND_XP:
            return snprintf(dest, size, "X+");
        case OPERAND_MX:
            return snprintf(dest, size, "-X");
        case OPERAND_Y:
            return snprintf(dest, size, "Y");
        case OPERAND_YP:
            return snprintf(dest, size, "Y+");
        case OPERAND_MY:
            return snprintf(dest, size, "-Y");
        case OPERAND_Z:
            return snprintf(dest, size, "Z");
        case OPERAND_ZP:
            return snprintf(dest, size, "Z+");
        case OPERAND_MZ:
            return snprintf(dest, size, "-Z");
        case OPERAND_YPQ:
            return snprintf(dest, size, "Y+%d", instructionDisasm->operandDisasms[index]);
        case OPERAND_ZPQ:
            return snprintf(dest, size, "Z+%d", instructionDisasm->operandDisasms[index]);
        case OPERAND_DATA:
            if (flags & PRINT_FLAG_DATA_BIN) {
                /* Data representation binary */
                char binary[9];
                int i;
                for (i = 0; i < 8; i++) {
                    if (instructionDisasm->operandDisasms[index] & (1 << i))
                        binary[7-i] = '1';
                    else
                        binary[7-i] = '0';
                }
                binary[8] = '\0';
                return snprintf(dest, size, AVR_FORMAT_OP_DATA_BIN("%s"), binary);
            } else if (flags & PRINT_FLAG_DATA_DEC) {
                /* Data representation decimal */
                return snprintf(dest, size, AVR_FORMAT_OP_DATA_DEC("%d"), instructionDisasm->operandDisasms[index]);
            } else {
                /* Default to data representation hex */
                return snprintf(dest, size, AVR_FORMAT_OP_DATA_HEX("%02x"), instructionDisasm->operandDisasms[index]);
            }
        case OPERAND_LONG_ABSOLUTE_ADDRESS:
            /* Divide the address by two to render a word address */
            return snprintf(dest, size, AVR_FORMAT_OP_ABSOLUTE_ADDRESS("%0*x"), AVR_ADDRESS_WIDTH, instructionDisasm->operandDisasms[index] / 2);
        case OPERAND_BRANCH_ADDRESS:
        case OPERAND_RELATIVE_ADDRESS:
            /* If we have address labels turned on, replace the relative
             * address with the appropriate address label */
            if (flags & PRINT_FLAG_ASSEMBLY) {
                return snprintf(dest, size, AVR_FORMAT_OP_ADDRESS_LABEL("%0*x"), AVR_ADDRESS_WIDTH, instructionDisasm->operandDisasms[index] + instructionDisasm->address + 2);
            } else {
                /* Print a plus sign for positive relative addresses, printf
                 * will insert a minus sign for negative relative addresses. */
                if (instructionDisasm->operandDisasms[index] >= 0) {
                    return snprintf(dest, size, AVR_FORMAT_OP_RELATIVE_ADDRESS("+%d"), instructionDisasm->operandDisasms[index]);
                } else {
                    return snprintf(dest, size, AVR_FORMAT_OP_RELATIVE_ADDRESS("%d"), instructionDisasm->operandDisasms[index]);
                }
            }
        default:
            break;
    }

    return 0;
}

int avr_instruction_get_str_comment(struct instruction *instr, char *dest, int size, int flags) {
    struct avrInstructionDisasm *instructionDisasm = (struct avrInstructionDisasm *)instr->data;
    int i;

    for (i = 0; i < instructionDisasm->instructionInfo->numOperands; i++) {
        if ( instructionDisasm->instructionInfo->operandTypes[i] == OPERAND_BRANCH_ADDRESS ||
             instructionDisasm->instructionInfo->operandTypes[i] == OPERAND_RELATIVE_ADDRESS) {
            return snprintf(dest, size, "; " AVR_FORMAT_OP_ABSOLUTE_ADDRESS("%x"), instructionDisasm->operandDisasms[i] + instructionDisasm->address + 2);
        }
    }

    return 0;
}

void avr_instruction_free(struct instruction *instr) {
    free(instr->data);
    instr->data = NULL;
}

/******************************************************************************/
/* AVR Directives */
/******************************************************************************/

unsigned int avr_directive_get_num_operands(struct instruction *instr) {
    return 1;
}

int avr_directive_get_str_mnemonic(struct instruction *instr, char *dest, int size, int flags) {
    struct avrDirective *directive = (struct avrDirective *)instr->data;
    return snprintf(dest, size, "%s", directive->name);
}

int avr_directive_get_str_operand(struct instruction *instr, char *dest, int size, int index, int flags) {
    struct avrDirective *directive = (struct avrDirective *)instr->data;

    if (strcmp(directive->name, AVR_DIRECTIVE_NAME_ORIGIN) == 0 && index == 0)
        return snprintf(dest, size, AVR_FORMAT_OP_ABSOLUTE_ADDRESS("%0*x"), AVR_ADDRESS_WIDTH, directive->value);

    return 0;
}

void avr_directive_free(struct instruction *instr) {
    free(instr->data);
    instr->data = NULL;
}

