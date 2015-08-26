#define N 1024
#define M_PI 3.14159265358979323846
#include <fftw3.h>
#include <math.h>
#include <stdio.h>

int main(){
  double *in, *in2;
  fftw_complex *out;
  fftw_plan pfft;
  fftw_plan prfft;
  in = (double*) fftw_malloc(sizeof(double) * N);
  in2 = (double*) fftw_malloc(sizeof(double) * N);
  out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
  pfft = fftw_plan_dft_r2c_1d(N, in, out, FFTW_ESTIMATE);
  prfft = fftw_plan_dft_c2r_1d(N, out, in2, FFTW_ESTIMATE);
  double max = 0;
  for (int i = 0; i < N; ++i){
    in[i] = 30.0 * sin(2.0 * M_PI * (double)i / 1024.0);
    if (max < in[i])
      max = in[i];
  }
  printf("-----------------in\n");
  for (int i = 0; i < N; ++i)
    printf("%d: %e   ", i, in[i]);
  printf("\n\n\n\n\n\n");
  fftw_execute(pfft);
  printf("-----------------out\n");
  for (int i = 0; i < N; ++i) 
      printf("%d: %e   ", i, out[i][0]); 
  printf("\n\n\n\n\n\n"); 
  
  fftw_execute(prfft);

  printf("-----------------in2\n");
  double max2 = 0;
  for (int i = 0; i < N; ++i){
    if(max2 < in2[i])
      max2 = in2[i];
    printf("%d: %e   ", i, in2[i]);
  }
  printf("\n\n\n\n\n\n"); 
  printf ("max in %e\n", max);
  printf ("max in2 %e\n", max2);
  fftw_destroy_plan(pfft);
  fftw_destroy_plan(prfft);
  fftw_free(in);
  fftw_free(out);
  return 0;
}
