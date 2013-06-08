#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <bytestream.h>
#include <disasmstream.h>
#include <printstream.h>

/* File ByteStream Support */
#include "file/file_support.h"
/* DisasmStream Support */
#include "avr/avr_support.h"
#include "pic/pic_support.h"
#include "8051/8051_support.h"
/* File PrintStream Support */
#include "printstream_file.h"

/* Debugging Unit Tests */
#include <file/test/test_bytestream.h>
#include <avr/test/test_avr.h>
#include <pic/test/test_pic.h>
#include <8051/test/test_8051.h>

/* Supported file types */
enum {
    FILE_TYPE_ATMEL_GENERIC,
    FILE_TYPE_INTEL_HEX,
    FILE_TYPE_MOTOROLA_SRECORD,
    FILE_TYPE_BINARY,
    FILE_TYPE_ASCII_HEX,
    FILE_TYPE_ELF
};

/* Supported architectures */
enum {
    ARCH_AVR8,
    ARCH_PIC_BASELINE,
    ARCH_PIC_MIDRANGE,
    ARCH_PIC_MIDRANGE_ENHANCED,
    ARCH_PIC_PIC18,
    ARCH_8051,
};

/* Supported data constant bases */
enum {
    DATA_BASE_HEX,
    DATA_BASE_BIN,
    DATA_BASE_DEC,
};

/* getopt flags for some long options that don't have a short option equivalent */
static int flag_no_addresses = 0;            /* Flag for --no-addresses */
static int flag_no_destination_comments = 0; /* Flag for --no-destination-comments */
static int flag_no_opcodes = 0;              /* Flag for --no-opcodes */
static int flag_assembly = 0;                /* Flag for --assembly */
static int flag_debug = 0;                   /* Flag for --debug */
static int flag_data_base = 0;               /* Base of data constants (hexadecimal, binary, decimal) */

static struct option long_options[] = {
    {"architecture", required_argument, NULL, 'a'},
    {"file-type", required_argument, NULL, 't'},
    {"out-file", required_argument, NULL, 'o'},
    {"assembly", no_argument, &flag_assembly, 1},
    {"data-base-hex", no_argument, &flag_data_base, DATA_BASE_HEX},
    {"data-base-bin", no_argument, &flag_data_base, DATA_BASE_BIN},
    {"data-base-dec", no_argument, &flag_data_base, DATA_BASE_DEC},
    {"no-opcodes", no_argument, &flag_no_opcodes, 1},
    {"no-addresses", no_argument, &flag_no_addresses, 1},
    {"no-destination-comments", no_argument, &flag_no_destination_comments, 1},
    {"debug", no_argument, &flag_debug, 1},
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {NULL, 0, NULL, 0}
};

void debug_tests(FILE *in, int file_type) {
    int success = 1;

    /* Test file bytestream parsing */
    if (in != NULL) {
        switch (file_type) {
            case FILE_TYPE_ATMEL_GENERIC:
                if (test_bytestream(in, bytestream_generic_init, bytestream_generic_close, bytestream_generic_read))
                    success = 0;
                break;
            case FILE_TYPE_INTEL_HEX:
                if (test_bytestream(in, bytestream_ihex_init, bytestream_ihex_close, bytestream_ihex_read))
                    success = 0;
                break;
            case FILE_TYPE_MOTOROLA_SRECORD:
                if (test_bytestream(in, bytestream_srecord_init, bytestream_srecord_close, bytestream_srecord_read))
                    success = 0;
                break;
            case FILE_TYPE_BINARY:
                if (test_bytestream(in, bytestream_binary_init, bytestream_binary_close, bytestream_binary_read))
                    success = 0;
                break;
            case FILE_TYPE_ASCII_HEX:
                if (test_bytestream(in, bytestream_asciihex_init, bytestream_asciihex_close, bytestream_asciihex_read))
                    success = 0;
                break;
            case FILE_TYPE_ELF:
                if (test_bytestream(in, bytestream_elf_init, bytestream_elf_close, bytestream_elf_read))
                    success = 0;
                break;
        }
    }

    /* Test AVR Architecture */
    if (test_disasm_avr_unit_tests()) success = 0;
    if (test_print_avr_unit_tests()) success = 0;

    /* Test PIC Architecture */
    if (test_disasm_pic_unit_tests()) success = 0;
    if (test_print_pic_unit_tests()) success = 0;

    /* Test 8051 Architecture */
    if (test_disasm_8051_unit_tests()) success = 0;
    if (test_print_8051_unit_tests()) success = 0;

    if (success)
        printf("All tests passed!\n");
    else
        printf("Some tests failed...\n");
}

static void print_usage(const char *programName) {
    printf("Usage: %s -a <architecture> [option(s)] <file>\n", programName);
    printf("Disassembles program file <file>. Use - for standard input.\n\n");
    printf("ucdisasm version 1.0 - 02/04/2013.\n");
    printf("Written by Vanya A. Sergeev - <vsergeev@gmail.com>.\n\n");
    printf("Additional Options:\n\
  -a, --architecture <arch>     Architecture to disassemble for.\n\
\n\
  -o, --out-file <file>         Write to file instead of standard output.\n\
\n\
  -t, --file-type <type>        Specify file type of the program file.\n\
\n\
  --assembly                    Produce assemble-able code with address labels.\n\
\n\
  --data-base-hex               Represent data constants in hexadecimal\n\
                                  (default).\n\
  --data-base-bin               Represent data constants in binary.\n\
  --data-base-dec               Represent data constants in decimal.\n\
\n\
  --no-addresses                Do not display address alongside disassembly.\n\
  --no-opcodes                  Do not display original opcode alongside\n\
                                  disassembly.\n\
  --no-destination-comments     Do not display destination address comments\n\
                                  of relative branch/jump/call instructions.\n\
\n\
  -h, --help                    Display this usage/help.\n\
  -v, --version                 Display the program's version.\n\
  --debug                       Run debugging tests.\n\n");
    printf("Supported architectures:\n\
  Atmel AVR8                avr\n\
  PIC Baseline              pic-baseline\n\
  PIC Midrange              pic-midrange\n\
  PIC Midrange Enhanced     pic-enhanced\n\
  PIC PIC18                 pic-18\n\
  8051                      8051\n\n");
    printf("Supported file types:\n\
  Atmel Generic             generic\n\
  Intel HEX8                ihex\n\
  Motorola S-Record         srec\n\
  Raw Binary                binary\n\
  ELF (64-bit)              elf\n\
  ASCII Hex                 ascii\n\n");
}

static void print_version(void) {
    printf("ucdisasm version 1.0 - 02/04/2013.\n");
    printf("Written by Vanya Sergeev - <vsergeev@gmail.com>\n");
}

void printstream_error_trace(struct PrintStream *ps, struct DisasmStream *ds, struct ByteStream *bs) {
    if (ps->error == NULL)
        fprintf(stderr, "\tPrint Stream Error: No error\n");
    else
        fprintf(stderr, "\tPrint Stream Error: %s\n", ps->error);

    if (ds->error == NULL)
        fprintf(stderr, "\tDisasm Stream Error: No error\n");
    else
        fprintf(stderr, "\tDisasm Stream Error: %s\n", ds->error);

    if (bs->error == NULL)
        fprintf(stderr, "\tByte Stream Error: No error\n");
    else
        fprintf(stderr, "\tByte Stream Error: %s\n", bs->error);

    fprintf(stderr, "\tPlease file an issue at https://github.com/vsergeev/vAVRdisasm/issues\n\tor email the author!\n\n");
}

int main(int argc, const char *argv[]) {
    /* User Options */
    int optc;
    char arch_str[16] = {0};
    char file_type_str[8] = {0};
    char file_out_str[4096] = {0};

    /* Input / Output files */
    FILE *file_in = NULL, *file_out = NULL;

    /* Disassembler Streams */
    int file_type = 0;
    int arch = 0;
    int flags = 0;
    struct ByteStream bs;
    struct DisasmStream ds;
    struct PrintStream ps;
    int ret;

    /* Parse command line options */
    while (1) {
        optc = getopt_long(argc, (char * const *)argv, "a:o:t:l:hv", long_options, NULL);
        if (optc == -1)
            break;
        switch (optc) {
            /* Long option */
            case 0:
                break;
            case 'a':
                strncpy(arch_str, optarg, sizeof(arch_str));
                break;
            case 't':
                strncpy(file_type_str, optarg, sizeof(file_type_str));
                break;
            case 'o':
                if (strcmp(optarg, "-") != 0)
                    strncpy(file_out_str, optarg, sizeof(file_out_str));
                break;
            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
            case 'v':
                print_version();
                exit(EXIT_SUCCESS);
            default:
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
        }
    }

    /* If there are no more arguments left but we're in debugging test mode */
    if (optind == argc && flag_debug) {
        debug_tests(file_in, 0);
        goto cleanup_exit_success;
    }

    /* If there are no more arguments left */
    if (optind == argc) {
        fprintf(stderr, "Error: No program file specified! Use - for standard input.\n\n");
        print_usage(argv[0]);
        goto cleanup_exit_failure;
    }

    /*** Open input file ***/

    /* Support reading from stdin with filename "-" */
    if (strcmp(argv[optind], "-") == 0) {
        file_in = stdin;
    } else {
    /* Otherwise, open the specified input file */
        file_in = fopen(argv[optind], "r");
        if (file_in == NULL) {
            perror("Error: Cannot open program file for disassembly");
            goto cleanup_exit_failure;
        }
    }

    /*** Determine architecture ***/

    if (arch_str[0] != '\0') {
        if (strcasecmp(arch_str, "avr") == 0)
            arch = ARCH_AVR8;
        else if (strcasecmp(arch_str, "pic-baseline") == 0)
            arch = ARCH_PIC_BASELINE;
        else if (strcasecmp(arch_str, "pic-midrange") == 0)
            arch = ARCH_PIC_MIDRANGE;
        else if (strcasecmp(arch_str, "pic-enhanced") == 0)
            arch = ARCH_PIC_MIDRANGE_ENHANCED;
        else if (strcasecmp(arch_str, "pic-18") == 0)
            arch = ARCH_PIC_PIC18;
        else if (strcasecmp(arch_str, "8051") == 0)
            arch = ARCH_8051;
        else {
            fprintf(stderr, "Unknown architecture %s.\n", arch_str);
            fprintf(stderr, "See program help/usage for supported architectures.\n");
            goto cleanup_exit_failure;
        }
    } else {
        fprintf(stderr, "Error: No architecture specified!\n\n");
        print_usage(argv[0]);
        goto cleanup_exit_failure;
    }

    /*** Determine input file type ***/

    /* If a file type was specified */
    if (file_type_str[0] != '\0') {
        if (strcasecmp(file_type_str, "generic") == 0)
            file_type = FILE_TYPE_ATMEL_GENERIC;
        else if (strcasecmp(file_type_str, "ihex") == 0)
            file_type = FILE_TYPE_INTEL_HEX;
        else if (strcasecmp(file_type_str, "srec") == 0)
            file_type = FILE_TYPE_MOTOROLA_SRECORD;
        else if (strcasecmp(file_type_str, "ascii") == 0)
            file_type = FILE_TYPE_ASCII_HEX;
        else if (strcasecmp(file_type_str, "binary") == 0)
            file_type = FILE_TYPE_BINARY;
        else if (strcasecmp(file_type_str, "elf") == 0)
            file_type = FILE_TYPE_ELF;
        else {
            fprintf(stderr, "Unknown file type %s.\n", file_type_str);
            fprintf(stderr, "See program help/usage for supported file types.\n");
            goto cleanup_exit_failure;
        }
    } else {
    /* Otherwise, attempt to auto-detect file type by first character */
        int c;
        c = fgetc(file_in);
        /* Intel HEX8 record statements start with : */
        if ((char)c == ':')
            file_type = FILE_TYPE_INTEL_HEX;
        /* Motorola S-Record record statements start with S */
        else if ((char)c == 'S')
            file_type = FILE_TYPE_MOTOROLA_SRECORD;
        /* Atmel Generic record statements start with a ASCII hex digit */
        else if ( ((char)c >= '0' && (char)c <= '9') || ((char)c >= 'a' && (char)c <= 'f') || ((char)c >= 'A' && (char)c <= 'F') )
            file_type = FILE_TYPE_ATMEL_GENERIC;
        else if (c == 0x7f)
            file_type = FILE_TYPE_ELF;
        else {
            fprintf(stderr, "Unable to auto-recognize file type by first character.\n");
            fprintf(stderr, "Please specify file type with -t / --file-type option.\n");
            goto cleanup_exit_failure;
        }
        ungetc(c, file_in);
    }

    /* Debug this file type if we're in debug mode */
    if (flag_debug) {
        debug_tests(file_in, file_type);
        goto cleanup_exit_success;
    }

    /*** Open output file ***/

    /* If an output file was specified */
    if (file_out_str[0] != '\0') {
        file_out = fopen(file_out_str, "w");
        if (file_out == NULL) {
            perror("Error opening output file for writing");
            goto cleanup_exit_failure;
        }
    } else {
    /* Otherwise, default the output file to stdout */
        file_out = stdout;
    }

    /*** Setup Formatting Flags ***/
    if (!flag_no_addresses)
        flags |= PRINT_FLAG_ADDRESSES;
    if (!flag_no_destination_comments)
        flags |= PRINT_FLAG_DESTINATION_COMMENT;
    if (!flag_no_opcodes)
        flags |= PRINT_FLAG_OPCODES;

    if (flag_data_base == DATA_BASE_BIN)
        flags |= PRINT_FLAG_DATA_BIN;
    else if (flag_data_base == DATA_BASE_DEC)
        flags |= PRINT_FLAG_DATA_DEC;
    else
        flags |= PRINT_FLAG_DATA_HEX;

    if (flag_assembly)
        flags |= PRINT_FLAG_ASSEMBLY;

    /*** Setup disassembler streams ***/

    /* Setup the ByteStream */
    bs.in = file_in;
    if (file_type == FILE_TYPE_ATMEL_GENERIC) {
        bs.stream_init = bytestream_generic_init;
        bs.stream_close = bytestream_generic_close;
        bs.stream_read = bytestream_generic_read;
    } else if (file_type == FILE_TYPE_INTEL_HEX) {
        bs.stream_init = bytestream_ihex_init;
        bs.stream_close = bytestream_ihex_close;
        bs.stream_read = bytestream_ihex_read;
    } else if (file_type == FILE_TYPE_MOTOROLA_SRECORD) {
        bs.stream_init = bytestream_srecord_init;
        bs.stream_close = bytestream_srecord_close;
        bs.stream_read = bytestream_srecord_read;
    } else if (file_type == FILE_TYPE_ASCII_HEX) {
        bs.stream_init = bytestream_asciihex_init;
        bs.stream_close = bytestream_asciihex_close;
        bs.stream_read = bytestream_asciihex_read;
    } else if (file_type == FILE_TYPE_ELF) {
        bs.stream_init = bytestream_elf_init;
        bs.stream_close = bytestream_elf_close;
        bs.stream_read = bytestream_elf_read;
    } else {
        bs.stream_init = bytestream_binary_init;
        bs.stream_close = bytestream_binary_close;
        bs.stream_read = bytestream_binary_read;
    }

    /* Setup the DisasmStream */
    ds.in = &bs;
    if (arch == ARCH_AVR8) {
        ds.stream_init = disasmstream_avr_init;
        ds.stream_close = disasmstream_avr_close;
        ds.stream_read = disasmstream_avr_read;
    } else if (arch == ARCH_PIC_BASELINE) {
        ds.stream_init = disasmstream_pic_baseline_init;
        ds.stream_close = disasmstream_pic_baseline_close;
        ds.stream_read = disasmstream_pic_baseline_read;
    } else if (arch == ARCH_PIC_MIDRANGE) {
        ds.stream_init = disasmstream_pic_midrange_init;
        ds.stream_close = disasmstream_pic_midrange_close;
        ds.stream_read = disasmstream_pic_midrange_read;
    } else if (arch == ARCH_PIC_MIDRANGE_ENHANCED) {
        ds.stream_init = disasmstream_pic_midrange_enhanced_init;
        ds.stream_close = disasmstream_pic_midrange_enhanced_close;
        ds.stream_read = disasmstream_pic_midrange_enhanced_read;
    } else if (arch == ARCH_PIC_PIC18) {
        ds.stream_init = disasmstream_pic_pic18_init;
        ds.stream_close = disasmstream_pic_pic18_close;
        ds.stream_read = disasmstream_pic_pic18_read;
    } else if (arch == ARCH_8051) {
        ds.stream_init = disasmstream_8051_init;
        ds.stream_close = disasmstream_8051_close;
        ds.stream_read = disasmstream_8051_read;
    }

    /* Setup the File PrintStream */
    ps.in = &ds;
    ps.stream_init = printstream_file_init;
    ps.stream_close = printstream_file_close;
    ps.stream_read = printstream_file_read;

    /* Initialize streams */
    if ((ret = ps.stream_init(&ps, flags)) < 0) {
        fprintf(stderr, "Error initializing streams! Error code: %d\n", ret);
        printstream_error_trace(&ps, &ds, &bs);
        goto cleanup_exit_failure;
    }

    /* Read from Print Stream until EOF */
    while ( (ret = ps.stream_read(&ps, file_out)) != STREAM_EOF ) {
        if (ret < 0) {
            fprintf(stderr, "Error occured during disassembly! Error code: %d\n", ret);
            printstream_error_trace(&ps, &ds, &bs);
            goto cleanup_exit_failure;
        }
    }

    /* Close streams */
    if ((ret = ps.stream_close(&ps)) < 0) {
        fprintf(stderr, "Error closing streams! Error code: %d\n", ret);
        printstream_error_trace(&ps, &ds, &bs);
        goto cleanup_exit_failure;
    }

    cleanup_exit_success:
    if (file_out != stdout && file_out != NULL)
        fclose(file_out);
    exit(EXIT_SUCCESS);

    cleanup_exit_failure:
    if (file_in != stdin && file_in != NULL)
        fclose(file_in);
    if (file_out != stdout && file_out != NULL)
        fclose(file_out);
    exit(EXIT_FAILURE);
}

