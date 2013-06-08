#include <elf.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include <bytestream.h>

// TODO: fix indentation

struct bytestream_elf_state {
    /* Memory mapped ELF file */
    Elf64_Ehdr *elf;
    /* Pointer to .text data in memory map */
    uint8_t *text;
    /* Instruction address */
    uint64_t address_current;
    uint64_t address_end;
    /* Size of ELF file */
    long size;
};

/* Finds the section header by name, or NULL if not found. */
Elf64_Shdr *find_sh(Elf64_Ehdr *elf, char *name) {
    Elf64_Shdr *shtab;
    char *strtab;
    int i;

    shtab = (void *)elf + elf->e_shoff;
    strtab = (void *)elf + shtab[elf->e_shstrndx].sh_offset;

    for (i = 0; i < elf->e_shnum; i++) {
        Elf64_Shdr *sh = &(shtab[i]);
        char *found = strtab + sh->sh_name;
        if (strcmp(found, name) == 0)
            return sh;
    }

    return NULL;
}

/* Returns pointer to memory mapped ELF file. */
Elf64_Ehdr *mmap_elf(FILE *fp, long size) {
    int fd;

    fd = fileno(fp);
    if (fd < 0)
        return NULL;
    return mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
}

long filesize(FILE *fp) {
    long size;

    if (fseek(fp, 0, SEEK_END) < 0)
        return -1;
    size = ftell(fp);
    rewind(fp);

    return size;
}

/* Init function */
int bytestream_elf_init(struct ByteStream *self) {
    struct bytestream_elf_state *state;

    state = self->state = calloc(1, sizeof(struct bytestream_elf_state));
    if (state == NULL) {
        self->error = "Error allocating opcode stream state!";
        return STREAM_ERROR_ALLOC;
    }

    state->size = filesize(self->in);
    if (state->size < 0) {
        self->error = "Error getting size of file!";
        return STREAM_ERROR_ALLOC;
    }

    state->elf = mmap_elf(self->in, state->size);
    if (state->elf == NULL) {
        self->error = "mmap() failed!";
        return STREAM_ERROR_ALLOC;
    }

    Elf64_Shdr *text_sh = find_sh(state->elf, ".text");
    if (text_sh == NULL) {
        self->error = ".text section not found!";
        return STREAM_ERROR_ALLOC;
    }
    state->text = (uint8_t *)state->elf + text_sh->sh_offset;
    state->address_current = text_sh->sh_addr;
    state->address_end = text_sh->sh_addr + text_sh->sh_size;

    self->error = NULL;

    return 0;
}

/* Close function */
int bytestream_elf_close(struct ByteStream *self) {
    struct bytestream_elf_state *state = self->state;

    munmap(state->elf, state->size); // TODO check error
    free(state);
    fclose(self->in);

    return 0;
}

/* Output function */
int bytestream_elf_read(struct ByteStream *self, uint8_t *data, uint32_t *address) {
    struct bytestream_elf_state *state = self->state;

    if (state->address_current == state->address_end)
        return STREAM_EOF;

    *data = *state->text++;
    *address = state->address_current++;

    return 0;
}
