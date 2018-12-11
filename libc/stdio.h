/*
 * Basic stdio.h, e.g. for DOS.
 * Copyright (C) 2018 René Rebe, ExactCODE; Germany.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * Alternatively, commercial licensing options are available from the
 * copyright holder ExactCODE GmbH Germany.
 */

/* TODO:
 * obbiously more printf format modifiers
 * convert to snprintf, also internally by default
 */

#ifndef __STDIO_H
#define __STDIO_H

#if defined(DOS)

#include "ctype.h"

#define EOF -1
typedef int FILE;
static FILE* stdin = (FILE*)1;
static FILE* stdout = (FILE*)1;
static FILE* stderr = (FILE*)1;

// 1st and early for debug prints, ...
int printf(const char* format, ...)
  __attribute__ ((format (printf, 1, 2)));


int sprintf(char* out, const char* format, ...)
  __attribute__ ((format (printf, 2, 3)));
int vsprintf(char* out, const char* format, va_list argp)
{
  char* _out = out;
  char temp[12]; // temp. formating buffer
  while (*format) {
    if (*format == '%') {
      ++format;
      
      // TODO: >9 (single digit) format and precision!
      // TODO: and support for all formats, ...
      uint8_t precision = 0, width = 0;
      if (isdigit(*format)) {
	width = *format++ - '0';
      }
      if (format[0] == '.' && isdigit(format[1])) {
	precision = format[1] - '0';
	format += 2;
      }
      
      if (*format == '%') {
        *out++ = '%';
      } else if (*format == 'c') {
        char char_to_print = va_arg(argp, int);
        *out++ = char_to_print;
      } else if (*format == 's') {
        char* str_to_print = va_arg(argp, char*);
        while (*str_to_print) {
	  *out++ = *str_to_print++;
	  if (precision && --precision == 0) break;
	}
      } else if (*format == 'd' || (*format == 'i'))  {
        uint8_t tempi = sizeof(temp); temp[--tempi] = 0;
	int int_to_print = va_arg(argp, int);
	if (int_to_print < 0) {
	  *out++ = '-';
	  int_to_print = -int_to_print;
	}
	// format int
	do {
	  int _ = int_to_print % 10;
	  temp[--tempi] = '0' + _;
	  int_to_print /= 10;
	} while (int_to_print);
	out += sprintf(out, temp + tempi);
      } else if (*format == 'u')  {
        uint8_t tempi = sizeof(temp); temp[--tempi] = 0;
	unsigned int_to_print = va_arg(argp, unsigned);
	// format unsigned
	do {
	  int _ = int_to_print % 10;
	  temp[--tempi] = '0' + _;
	  int_to_print /= 10;
	} while (int_to_print);
	out += sprintf(out, temp + tempi);
      } else if (*format == 'x' || *format == 'p') {
	uint8_t tempi = sizeof(temp); temp[--tempi] = 0;
	unsigned hex_to_print = va_arg(argp, int);
	// hex format int
	do {
	  int _ = hex_to_print & 0xf;
	  if (_ > 9) _ += 'a' - '9' - 1;
	  temp[--tempi] = '0' + _;
	  hex_to_print >>= 4;
	} while (hex_to_print);
	out += sprintf(out, temp + tempi);
      } else if (*format == 'f')  {
        float float_to_print = va_arg(argp, double);
	// format float - TODO: all the really complex stuff, ...
	int _ = float_to_print;
	unsigned __ = (float_to_print * 10);
	out += sprintf(out, "%d.%d", _, __ % 10);
      } else {
        out += sprintf(out, "NIY: %%%c", *format);
      }
    } else {
#ifdef DOS
      // TODO: only for non-binary streams, sigh!
      if (*format == '\n')
	*out++ = '\r';
#endif
      *out++ = *format;
    }
    ++format;
  }

  *out = 0;
  return out - _out;
}

int sprintf(char* text, const char* format, ...)
  __attribute__ ((format (printf, 2, 3)));
int sprintf(char* text, const char* format, ...)
{
  va_list argp, argp2;
  va_start(argp, format);
  va_copy(argp2, argp);
  int ret = vsprintf(text, format, argp2);
  va_end(argp2);
  
  return ret;
}

int fscanf(FILE *stream, const char *format, ...);


int fprintf(FILE* stream, const char* format, ...)
  __attribute__ ((format (printf, 2, 3)));
int fprintf(FILE* stream, const char* format, ...)
{
  char text[80]; // TODO: dynamic!
  
  va_list argp, argp2;
  va_start(argp, format);
  va_copy(argp2, argp);
  int ret = vsprintf(text, format, argp2);
  va_end(argp2);
  
  write((int)stream, text, strlen(text));
  return ret;
}

int printf(const char* format, ...)
{
  char text[80]; // TODO: dynamic!
  
  va_list argp, argp2;
  va_start(argp, format);
  va_copy(argp2, argp);
  int ret = vsprintf(text, format, argp2);
  va_end(argp2);
  
  write(1, text, strlen(text));
  return ret;
}


int nprintf(const char* format, ...)
  __attribute__ ((format (printf, 1, 2)));
int nprintf(const char* format, ...)
{
  char text[80]; // TODO: dynamic!
  
  va_list argp, argp2;
  va_start(argp, format);
  va_copy(argp2, argp);
  int ret = vsprintf(text, format, argp2);
  va_end(argp2);
  
  write(1, text, strlen(text));
  return ret;
}


size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);

size_t fwrite(const void* ptr, size_t size, size_t nmemb,
	      FILE* stream)
{
  return write((int)stream, (const char*)ptr, nmemb * size);
}

int fputc(int c, FILE* stream)
{
  char b = c;
  return fwrite(&b, 1, 1, stream);
}

int fgetc(FILE* stream)
{
}

int ungetc(int c, FILE *stream)
{
}

int fflush(FILE* stream)
{
}

#else
#include_next <stdio.h>
#endif

#endif // __STDIO_H
