#if defined(WIN32) || defined(DOS)

#define M_PI	3.14159265358979323846	/* pi */

double sin(double x) {
  asm ("fldl %0;"
       "fsin;"
       "fstpl %0" : "+m"(x));
  return x;
}

double cos(double x) {
  asm ("fldl %0;"
       "fcos;"
       "fstpl %0" : "+m"(x));
  return x;
}

double sqrt(double x) {
  asm ("fldl %0;"
       "fsqrt;"
       "fstpl %0" : "+m"(x));
  return x;
}

double fabs(double x) {
  /*
  asm ("fldl %0;"
       "fabs;"
       "fstpl %0" : "+m"(x));
  return x;
  */
  return x >= 0 ? x : -x;
}

float sinf(float x) {
  asm ("flds %0;"
       "fsin;"
       "fstps %0" : "+m"(x));
  return x;
}

float cosf(float x) {
  asm ("flds %0;"
       "fcos;"
       "fstps %0" : "+m"(x));
  return x;
}

float sqrtf(float x) {
  asm ("flds %0;"
       "fsqrt;"
       "fstps %0" : "+m"(x));
  return x;
}

float fabsf(float x) {
  /*
  asm ("flsd %0;"
       "fabs;"
       "fstps %0" : "+m"(x));
  return x;
  */
  return x >= 0 ? x : -x;
}

#else
#include_next <inttypes.h>
#endif
