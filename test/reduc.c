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
double reduc(double *restrict a, unsigned long long n)
{
  //
  double r = 0.0;

  //
  for (unsigned long long i = 0; i < n; i++)
    r += a[i];

  //
  return r;
}

//
int main(int argc, char **argv)
{
  //
  if (argc < 2)
    return printf("Usqge: %s [n]\n", argv[0]), 2;

  //
  unsigned long long n = atoll(argv[1]);
  unsigned long long s = sizeof(double) * n;
  
  //
  double *restrict a = _mm_malloc(s, ALIGN);

  //
  init(a, 1.0, n);

  //
  double r = reduc(a, n);

  //
  printf("s = %llu MiB; r = %lf\n", s >> 20, r);
  
  //
  _mm_free(a);
  
  return 0;
}
