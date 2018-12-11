/*
 * Simple string.h, e.g. for DOS.
 * Copyright (C) 2018 René Rebe, ExactCODE; Germany.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * Alternatively, commercial licensing options are available from the
 * copyright holder ExactCODE GmbH Germany.
 */

#if defined(WIN32) || defined(DOS)

void* memset(void* s, int c, size_t n) {
  uint8_t* d = (uint8_t*)s;
  while (n-- > 0)
    *d = c;
  return s;
}

void* memcpy(void* dest, const void* src, size_t n){
  uint8_t* d = (uint8_t*)dest, *s = (uint8_t*)src;
  while (n-- > 0)
    *d++ = *s++;
  return dest;
}

/*
int memcmp(const void* s1, const void* s2, size_t n) {
}
*/

char* strcpy(char* dest, const char* src) {
  while ((*dest = *src))
    ++dest, ++src;
  return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
  while (n-- > 0 && (*dest = *src))
    ++dest, ++src;
  return dest;
}

int strcmp(const char* s1, const char* s2) {
  /* as long as string and equal */
  while (*s1 && *s2 && *s1 == *s2) {
    ++s1; ++s2;
  }
  return *s1 - *s2;
}

/*

int strncmp(const char* s1, const char* s2, size_t n) {
  return 1;
}
*/

#else
#include_next <string.h>
#endif
