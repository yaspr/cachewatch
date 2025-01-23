//
#include <stdio.h>
#include <stdlib.h>
#include <emmintrin.h>

//
#define ALIGN 32

//
void init(double *restrict a, double c, unsigned long long n)
{
  //
  for (unsigned long long i = 0; i < n; i++)
    a[i] = c;
}

//
double dotprod(double *restrict a, double *restrict b, unsigned long long n)
{
  //
  double d = 0.0;

  //
  for (unsigned long long i = 0; i < n; i++)
    d += a[i] * b[i];

  //
  return d;
}

//
int main(int argc, char **argv)
{
  //
  if (argc < 2)
    return printf("Usage: %s [n]\n", argv[0]), 2;

  //
  unsigned long long n = atoll(argv[1]);
  unsigned long long s = sizeof(double) * n;

  //
  double *restrict a = _mm_malloc(s, ALIGN);
  double *restrict b = _mm_malloc(s, ALIGN);

  //
  init(a, 1.0, n);
  init(b, 2.0, n);

  //
  double d = dotprod(a, b, n);

  //
  printf("s = %llu MiB; d = %lf\n", s >> 19, d);
  
  //
  _mm_free(a);
  _mm_free(b);
  
  return 0;
}
