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
void gemm_ikj(double *restrict a,
	      double *restrict b,
	      double *restrict c,
	      unsigned long long n)
{
  for (unsigned long long i = 0; i < n; i++)
    for (unsigned long long k = 0; k < n; k++)
      {
	double _a_ = a[i * n + k];
	
	for (unsigned long long j = 0; j < n; j++)
	  c[i * n + j] += _a_ * b[k * n + j];
      }
}

//
int main(int argc, char **argv)
{
  //
  if (argc < 2)
    return printf("Usage: %s [n]\n", argv[0]), 2;

  //
  unsigned long long n = atoll(argv[1]);
  unsigned long long s = sizeof(double) * n * n;

  //
  double *restrict a = _mm_malloc(s, ALIGN);
  double *restrict b = _mm_malloc(s, ALIGN);
  double *restrict c = _mm_malloc(s, ALIGN);

  //
  init(a, 1.0, n * n);
  init(b, 2.0, n * n);
  init(c, 0.0, n * n);
  
  //
  gemm_ikj(a, b, c, n);

  //
  printf("s = %llu MiB; c[0] = %lf; c[%llu] = %lf\n",
	 (s * 3) >> 20,
	 c[0],
	 n * n - 1,
	 c[n * n - 1]);
  
  //
  _mm_free(a);
  _mm_free(b);
  _mm_free(c);
  
  return 0;
}
