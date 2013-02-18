CC = gcc
CFLAGS = -Wall -g -D_GNU_SOURCE -I.
#CFLAGS = -Wall -O3 -D_GNU_SOURCE -I.
LDFLAGS=
LIBGIS_OBJECTS = file/libGIS-1.0.5/atmel_generic.o file/libGIS-1.0.5/ihex.o file/libGIS-1.0.5/srecord.o
FILE_OBJECTS = $(LIBGIS_OBJECTS) file/atmel_generic.o file/ihex.o file/srecord.o file/binary.o file/debug.o file/asciihex.o file/test/test_bytestream.o
AVR_OBJECTS = avr/avr_instruction_set.o avr/avr_disasm.o avr/avr_accessors.o avr/test/test_disasm_avr.o avr/test/test_print_avr.o
PIC_OBJECTS = pic/pic_instruction_set.o pic/pic_disasm.o pic/pic_accessors.o pic/test/test_disasm_pic.o pic/test/test_print_pic.o
a8051_OBJECTS = 8051/8051_instruction_set.o 8051/8051_disasm.o 8051/8051_accessors.o 8051/test/test_disasm_8051.o 8051/test/test_print_8051.o
PRINT_OBJECTS = printstream_file.o
OBJECTS = $(FILE_OBJECTS) $(AVR_OBJECTS) $(PIC_OBJECTS) $(PRINT_OBJECTS) $(a8051_OBJECTS) main.o

PROGNAME = ucdisasm
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

all: $(PROGNAME)

install: $(PROGNAME)
	install -D -s -m 0755 $(PROGNAME) $(DESTDIR)$(BINDIR)/$(PROGNAME)

$(PROGNAME): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

clean:
	rm -rf $(PROGNAME) $(OBJECTS)

test: $(PROGNAME)
	python2 crazy_test.py

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(PROGNAME)

