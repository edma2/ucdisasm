#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <printstream.h>
#include <instruction.h>
#include <printstream_file.h>

/******************************************************************************/
/* File Print Stream Support */
/******************************************************************************/

/* Print Stream State */
struct printstream_file_state {
    /* Print Option Bit Flags */
    unsigned int flags;
};

int printstream_file_init(struct PrintStream *self, int flags) {
    /* Allocate stream state */
    self->state = malloc(sizeof(struct printstream_file_state));
    if (self->state == NULL) {
        self->error = "Error allocating format stream state!";
        return STREAM_ERROR_ALLOC;
    }

    /* Initialize stream state */
    memset(self->state, 0, sizeof(struct printstream_file_state));
    ((struct printstream_file_state *)self->state)->flags = flags;

    /* Reset the error to NULL */
    self->error = NULL;

    /* Initialize the input stream */
    if (self->in->stream_init(self->in) < 0) {
        self->error = "Error in input stream initialization!";
        return STREAM_ERROR_INPUT;
    }

    return 0;
}

int printstream_file_close(struct PrintStream *self) {
    /* Free stream state memory */
    free(self->state);

    /* Close input stream */
    if (self->in->stream_close(self->in) < 0) {
        self->error = "Error in input stream close!";
        return STREAM_ERROR_INPUT;
    }

    return 0;
}

int printstream_file_read(struct PrintStream *self, FILE *out) {
    struct printstream_file_state *state = (struct printstream_file_state *)self->state;
    struct instruction instr;
    char str[128];
    int i, ret;

    /* Read a disassembled instruction */
    ret = self->in->stream_read(self->in, &instr);
    switch (ret) {
        case 0:
            break;
        case STREAM_EOF:
            return STREAM_EOF;
        default:
            self->error = "Error in disasm stream read!";
            return STREAM_ERROR_INPUT;
    }

    #define print_str() do { if (fputs(str, out) < 0) goto fputs_error; } while(0)
    #define print_comma() do { if (fputs(", ", out) < 0) goto fputs_error; } while(0)
    #define print_newline() do { if (fputs("\n", out) < 0) goto fputs_error; } while(0)
    #define print_tab() do { if (fputs("\t", out) < 0) goto fputs_error; } while (0)

    /* If the disassembly stream emitted a directive instead of an instruction */
    if (instr.type == DISASM_TYPE_DIRECTIVE) {
        /* If we're not outputting assembly, skip it */
        if (!(state->flags & PRINT_FLAG_ASSEMBLY)) {
            instr.free(&instr);
            return 0;
        }

        print_tab();

        /* Print the directive name */
        instr.get_str_mnemonic(&instr, str, sizeof(str), state->flags);
        print_str();
        print_tab();

        /* Print the directive operands */
        for (i = 0; ; i++) {
            /* Print this operand index i */
            if (instr.get_str_operand(&instr, str, sizeof(str), i, state->flags) > 0) {
                if (i > 0) print_comma();
                print_str();
            /* No more operands to print */
            } else {
                break;
            }
        }

        print_newline();

        /* Free the allocated directive */
        instr.free(&instr);

        return 0;
    }

    /* Print an address label if we're printing assembly */
    if (state->flags & PRINT_FLAG_ASSEMBLY) {
        instr.get_str_address_label(&instr, str, sizeof(str), state->flags);
        print_str();
        print_tab();
    /* Or print an normal address */
    } else if (state->flags & PRINT_FLAG_ADDRESSES) {
        instr.get_str_address(&instr, str, sizeof(str), state->flags);
        print_str();
        print_tab();
    }

    /* Print the opcodes */
    if (state->flags & PRINT_FLAG_OPCODES) {
        instr.get_str_opcodes(&instr, str, sizeof(str), state->flags);
        print_str();
        print_tab();
    }

    /* Print the mnemonic */
    instr.get_str_mnemonic(&instr, str, sizeof(str), state->flags);
    print_str();
    print_tab();

    /* Print the operands */
    for (i = 0; ; i++) {
        /* Print this operand index i */
        if (instr.get_str_operand(&instr, str, sizeof(str), i, state->flags) > 0) {
            if (i > 0) print_comma();
            print_str();
        /* No more operands to print */
        } else {
            break;
        }
    }

    /* Print a comment (e.g. destination address comment) */
    if (state->flags & PRINT_FLAG_DESTINATION_COMMENT) {
        if (instr.get_str_comment(&instr, str, sizeof(str), state->flags) > 0) {
            print_tab();
            print_str();
        }
    }

    /* Print a newline */
    print_newline();

    /* Free the allocated disassembled instruction */
    instr.free(&instr);

    return 0;

    fputs_error:
    self->error = "Error writing to output file!";
    return STREAM_ERROR_OUTPUT;
}

