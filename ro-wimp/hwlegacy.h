#ifndef ISPY_HWLEGACY_H
#define ISPY_HWLEGACY_H
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
void *hw_alloc(int size);
void hw_free(void *pointer);
void hw_abort(char *program, ...);
void hw_exit(int status);
void hw_service(void);
int hw_printf(const char *format, ...);
int hw_fprintf(FILE *stream, const char *format, ...);
int hw_puts(const char *text, FILE *stream);
int hw_putc(int c, FILE *stream);
int hw_putchar(int c);
int hw_flush(FILE *stream);
/* The legacy routines have no ownership boundary. All their allocations are
 * tracked until return (including exceptional paths and temporary routes).
 * Their console reports are suppressed; errors return through hw_abort. */
#define ISPY_CALLABLE 1
#define Malloc hw_alloc
#define AbortExit hw_abort
#define free hw_free
#define exit hw_exit
#undef printf
#undef fprintf
#undef fputs
#undef putc
#undef putchar
#undef fflush
#define printf hw_printf
#define fprintf hw_fprintf
#define fputs hw_puts
#define putc hw_putc
#define putchar hw_putchar
#define fflush hw_flush
#endif
