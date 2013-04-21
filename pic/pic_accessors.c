#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <instruction.h>
#include <printstream_file.h>

#include "pic_instruction_set.h"

/* PIC formats */
#define PIC_FORMAT_OP_REGISTER(fmt)             "" fmt "h"        /* clrf 25h */
#define PIC_FORMAT_OP_BIT(fmt)                  "" fmt ""         /* bsf 0x32, 0 */
#define PIC_FORMAT_OP_DATA_HEX(fmt)             "0x" fmt ""       /* movlw 0x6 */
#define PIC_FORMAT_OP_DATA_BIN(fmt)             "b'" fmt "'"      /* movlw b'00000110' */
#define PIC_FORMAT_OP_DATA_DEC(fmt)             "" fmt ""         /* movlw 6 */
#define PIC_FORMAT_OP_ABSOLUTE_ADDRESS(fmt)     "0x" fmt ""       /* call 0xb6 */
#define PIC_FORMAT_OP_RELATIVE_ADDRESS(fmt)     "." fmt ""        /* rcall .2 */
#define PIC_FORMAT_OP_ADDRESS_LABEL(fmt)        "A_" fmt ""       /* call A_0004 */
#define PIC_FORMAT_OP_BIT_RAM_DEST(fmt)         "" fmt ""         /* crlf 0x25, 1 */
#define PIC_FORMAT_OP_FSR_INDEX(fmt)            "FSR" fmt ""      /* lfsr FSR, 0x100 */
#define PIC_FORMAT_OP_INDF_INDEX(fmt)           "FSR" fmt ""      /* moviw 3[FSR1] */
#define PIC_FORMAT_OP_RAW_WORD(fmt)             "0x" fmt ""       /* dw 0xabcd */
#define PIC_FORMAT_OP_RAW_BYTE(fmt)             "0x" fmt ""       /* db 0xab */
#define PIC_FORMAT_ADDRESS(fmt)                 "" fmt ":"        /* 0004: */
#define PIC_FORMAT_ADDRESS_LABEL(fmt)           "A_" fmt ":"      /* A_0004: */

/* Address filed width, e.g. 4 -> 0x0004 */
#define PIC_ADDRESS_WIDTH               4

/* PIC Instruction Accessor Functions */
uint32_t pic_instruction_get_address(struct instruction *instr);
unsigned int pic_instruction_get_width(struct instruction *instr);
unsigned int pic_instruction_get_num_operands(struct instruction *instr);
unsigned int pic_instruction_get_opcodes(struct instruction *instr, uint8_t *dest);
int pic_instruction_get_str_address_label(struct instruction *instr, char *dest, int size, int flags);
int pic_instruction_get_str_address(struct instruction *instr, char *dest, int size, int flags);
int pic_instruction_get_str_opcodes(struct instruction *instr, char *dest, int size, int flags);
int pic_instruction_get_str_mnemonic(struct instruction *instr, char *dest, int size, int flags);
int pic_instruction_get_str_operand(struct instruction *instr, char *dest, int size, int index, int flags);
int pic_instruction_get_str_comment(struct instruction *instr, char *dest, int size, int flags);
void pic_instruction_free(struct instruction *instr);

/* PIC Directive Accessor Functions */
unsigned int pic_directive_get_num_operands(struct instruction *instr);
int pic_directive_get_str_mnemonic(struct instruction *instr, char *dest, int size, int flags);
int pic_directive_get_str_operand(struct instruction *instr, char *dest, int size, int index, int flags);
void pic_directive_free(struct instruction *instr);

/******************************************************************************/
/* PIC Instructions */
/******************************************************************************/

uint32_t pic_instruction_get_address(struct instruction *instr) {
    struct picInstructionDisasm *instructionDisasm = (struct picInstructionDisasm *)instr->data;
    return instructionDisasm->address;
}

unsigned int pic_instruction_get_width(struct instruction *instr) {
    struct picInstructionDisasm *instructionDisasm = (struct picInstructionDisasm *)instr->data;
    return instructionDisasm->instructionInfo->width;
}

unsigned int pic_instruction_get_num_operands(struct instruction *instr) {
    struct picInstructionDisasm *instructionDisasm = (struct picInstructionDisasm *)instr->data;
    return instructionDisasm->instructionInfo->numOperands;
}

unsigned int pic_instruction_get_opcodes(struct instruction *instr, uint8_t *dest) {
    struct picInstructionDisasm *instructionDisasm = (struct picInstructionDisasm *)instr->data;
    int i;

    for (i = 0; i < instructionDisasm->instructionInfo->width; i++)
        *dest++ = instructionDisasm->opcode[i];

    return instructionDisasm->instructionInfo->width;
}

int pic_instruction_get_str_address_label(struct instruction *instr, char *dest, int size, int flags) {
    struct picInstructionDisasm *instructionDisasm = (struct picInstructionDisasm *)instr->data;
    return snprintf(dest, size, PIC_FORMAT_ADDRESS_LABEL("%0*x"), PIC_ADDRESS_WIDTH, instructionDisasm->address);
}

int pic_instruction_get_str_address(struct instruction *instr, char *dest, int size, int flags) {
    struct picInstructionDisasm *instructionDisasm = (struct picInstructionDisasm *)instr->data;
    return snprintf(dest, size, PIC_FORMAT_ADDRESS("%*x"), PIC_ADDRESS_WIDTH, instructionDisasm->address);
}

int pic_instruction_get_str_opcodes(struct instruction *instr, char *dest, int size, int flags) {
    struct picInstructionDisasm *instructionDisasm = (struct picInstructionDisasm *)instr->data;

    if (instructionDisasm->instructionInfo->width == 1)
        return snprintf(dest, size, "%02x         ", instructionDisasm->opcode[0]);
    else if (instructionDisasm->instructionInfo->width == 2)
        return snprintf(dest, size, "%02x %02x      ", instructionDisasm->opcode[1], instructionDisasm->opcode[0]);
    else if (instructionDisasm->instructionInfo->width == 4)
        return snprintf(dest, size, "%02x %02x %02x %02x", instructionDisasm->opcode[3], instructionDisasm->opcode[2], instructionDisasm->opcode[1], instructionDisasm->opcode[0]);

    return 0;
}

int pic_instruction_get_str_mnemonic(struct instruction *instr, char *dest, int size, int flags) {
    struct picInstructionDisasm *instructionDisasm = (struct picInstructionDisasm *)instr->data;
    return snprintf(dest, size, "%s", instructionDisasm->instructionInfo->mnemonic);
}

int pic_instruction_get_str_operand(struct instruction *instr, char *dest, int size, int index, int flags) {
    struct picInstructionDisasm *instructionDisasm = (struct picInstructionDisasm *)instr->data;

    if (index < 0 || index > instructionDisasm->instructionInfo->numOperands - 1)
        return 0;

    /* Print the operand */
    switch (instructionDisasm->instructionInfo->operandTypes[index]) {
        case OPERAND_REGISTER:
            return snprintf(dest, size, PIC_FORMAT_OP_REGISTER("%x"), instructionDisasm->operandDisasms[index]);
            break;
        case OPERAND_BIT_RAM_DEST:
            if (instructionDisasm->operandDisasms[index] == 1)
                return snprintf(dest, size, "%d", instructionDisasm->operandDisasms[index]);
            else
                break;
            break;
        case OPERAND_BIT_REG_DEST:
            if (instructionDisasm->operandDisasms[index] == 0) {
                return snprintf(dest, size, "W");
            } else {
                return snprintf(dest, size, "F");
            }
            break;
        case OPERAND_BIT:
            return snprintf(dest, size, PIC_FORMAT_OP_BIT("%d"), instructionDisasm->operandDisasms[index]);
            break;
        case OPERAND_RAW_WORD:
            return snprintf(dest, size, PIC_FORMAT_OP_RAW_WORD("%04x"), instructionDisasm->operandDisasms[index]);
            break;
        case OPERAND_RAW_BYTE:
            return snprintf(dest, size, PIC_FORMAT_OP_RAW_BYTE("%02x"), instructionDisasm->operandDisasms[index]);
            break;
        case OPERAND_LONG_MOVFF_DATA_ADDRESS:
        case OPERAND_ABSOLUTE_DATA_ADDRESS:
        case OPERAND_LONG_ABSOLUTE_DATA_ADDRESS:
        case OPERAND_ABSOLUTE_PROG_ADDRESS:
        case OPERAND_LONG_ABSOLUTE_PROG_ADDRESS:
            /* If we have address labels turned on, replace the address with
             * the appropriate address label */
            if (flags & PRINT_FLAG_ASSEMBLY) {
                return snprintf(dest, size, PIC_FORMAT_OP_ADDRESS_LABEL("%0*x"), PIC_ADDRESS_WIDTH, instructionDisasm->operandDisasms[index]);
            } else {
                return snprintf(dest, size, PIC_FORMAT_OP_ABSOLUTE_ADDRESS("%0*x"), PIC_ADDRESS_WIDTH, instructionDisasm->operandDisasms[index]);
            }
            break;
        case OPERAND_LITERAL:
        case OPERAND_LONG_LFSR_LITERAL:
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
                return snprintf(dest, size, PIC_FORMAT_OP_DATA_BIN("%s"), binary);
            } else if (flags & PRINT_FLAG_DATA_DEC) {
                /* Data representation decimal */
                return snprintf(dest, size, PIC_FORMAT_OP_DATA_DEC("%d"), instructionDisasm->operandDisasms[index]);
            } else {
                /* Default to data representation hex */
                return snprintf(dest, size, PIC_FORMAT_OP_DATA_HEX("%02x"), instructionDisasm->operandDisasms[index]);
            }
            break;
        /* Mid-range Enhanced Operands */
        case OPERAND_RELATIVE_PROG_ADDRESS:
            /* If we have address labels turned on, replace the relative
             * address with the appropriate address label */
            if (flags & PRINT_FLAG_ASSEMBLY) {
                return snprintf(dest, size, PIC_FORMAT_OP_ADDRESS_LABEL("%0*x"), PIC_ADDRESS_WIDTH, instructionDisasm->operandDisasms[index] + instructionDisasm->address + 2);
            } else {
                /* Print a plus sign for positive relative addresses, printf
                 * will insert a minus sign for negative relative addresses. */
                if (instructionDisasm->operandDisasms[index] >= 0) {
                    return snprintf(dest, size, PIC_FORMAT_OP_RELATIVE_ADDRESS("+%d"), instructionDisasm->operandDisasms[index]);
                } else {
                    return snprintf(dest, size, PIC_FORMAT_OP_RELATIVE_ADDRESS("%d"), instructionDisasm->operandDisasms[index]);
                }
            }
            break;
        case OPERAND_FSR_INDEX:
            return snprintf(dest, size, PIC_FORMAT_OP_FSR_INDEX("%d"), instructionDisasm->operandDisasms[index]);
            break;

        case OPERAND_INDF_INDEX:
            if (index == 0 && instructionDisasm->instructionInfo->numOperands == 2) {
                /* OPERAND_INDF_INDEX, OPERAND_INCREMENT_MODE */
                if (instructionDisasm->instructionInfo->operandTypes[1] == OPERAND_INCREMENT_MODE) {
                    switch (instructionDisasm->operandDisasms[1]) {
                        case 0:
                            return snprintf(dest, size, "++" PIC_FORMAT_OP_INDF_INDEX("%d"), instructionDisasm->operandDisasms[0]);
                        case 1:
                            return snprintf(dest, size, "--" PIC_FORMAT_OP_INDF_INDEX("%d"), instructionDisasm->operandDisasms[0]);
                        case 2:
                            return snprintf(dest, size, PIC_FORMAT_OP_INDF_INDEX("%d") "++", instructionDisasm->operandDisasms[0]);
                        case 3:
                            return snprintf(dest, size, PIC_FORMAT_OP_INDF_INDEX("%d") "--", instructionDisasm->operandDisasms[0]);
                        default:
                            break;
                    }
                /* OPERAND_INDF_INDEX, OPERAND_SIGNED_LITERAL */
                } else if (instructionDisasm->instructionInfo->operandTypes[1] == OPERAND_SIGNED_LITERAL) {
                    return snprintf(dest, size, "%d[" PIC_FORMAT_OP_INDF_INDEX("%d") "]", instructionDisasm->operandDisasms[1], instructionDisasm->operandDisasms[0]);
                }
            }
            break;

        case OPERAND_SIGNED_LITERAL:
            /* If this was an OPERAND_INDF_INDEX, OPERAND_SIGNED_LITERAL, we
             * handled it in OPERAND_INDF_INDEX */
            if (index == 1 && instructionDisasm->instructionInfo->numOperands == 2 && instructionDisasm->instructionInfo->operandTypes[0] == OPERAND_INDF_INDEX)
                break;
            else
                return snprintf(dest, size, PIC_FORMAT_OP_DATA_DEC("%d"), instructionDisasm->operandDisasms[index]);
            break;
        case OPERAND_INCREMENT_MODE:
            /* Handled in OPERAND_INDF_INDEX */
            break;

        case OPERAND_BIT_FAST_CALLRETURN:
            if (instructionDisasm->operandDisasms[index] == 1)
                return snprintf(dest, size, "%d", instructionDisasm->operandDisasms[index]);
            else
                break;

        default:
            break;
    }

    /* Return 0 for no more operands */
    return 0;
}

int pic_instruction_get_str_comment(struct instruction *instr, char *dest, int size, int flags) {
    struct picInstructionDisasm *instructionDisasm = (struct picInstructionDisasm *)instr->data;
    int i;

    /* Print destination address comment */
    for (i = 0; i < instructionDisasm->instructionInfo->numOperands; i++) {
        if (instructionDisasm->instructionInfo->operandTypes[i] == OPERAND_RELATIVE_PROG_ADDRESS)
            return snprintf(dest, size, "; " PIC_FORMAT_OP_ABSOLUTE_ADDRESS("%0*x"), PIC_ADDRESS_WIDTH, instructionDisasm->operandDisasms[i] + instructionDisasm->address + 2);
    }

    return 0;
}

void pic_instruction_free(struct instruction *instr) {
    free(instr->data);
    instr->data = NULL;
}

/******************************************************************************/
/* PIC Directives */
/******************************************************************************/

/* Only ORG and END implemented for now */

unsigned int pic_directive_get_num_operands(struct instruction *instr) {
    struct picDirective *directive = (struct picDirective *)instr->data;
    if (strcmp(directive->name, PIC_DIRECTIVE_NAME_ORIGIN) == 0)
        return 1;
    return 0;
}

int pic_directive_get_str_mnemonic(struct instruction *instr, char *dest, int size, int flags) {
    struct picDirective *directive = (struct picDirective *)instr->data;
    return snprintf(dest, size, "%s", directive->name);
}

int pic_directive_get_str_operand(struct instruction *instr, char *dest, int size, int index, int flags) {
    struct picDirective *directive = (struct picDirective *)instr->data;

    if (strcmp(directive->name, PIC_DIRECTIVE_NAME_ORIGIN) == 0 && index == 0)
        return snprintf(dest, size, PIC_FORMAT_OP_ABSOLUTE_ADDRESS("%0*x"), PIC_ADDRESS_WIDTH, directive->value);

    return 0;
}

void pic_directive_free(struct instruction *instr) {
    free(instr->data);
    instr->data = NULL;
}

