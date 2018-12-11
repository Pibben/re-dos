/* Copyright (c) 2010 ExactCODE GmbH. All rights reserved. */

char* strtok_r(char *restrict str, const char *restrict sep, char **restrict lasts)
{
}

char* _lasts;
char* strtok(char *restrict str, const char *restrict sep)
{
  return strtok_r(str, sep, &_lasts;
}
