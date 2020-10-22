/**
 * @file random.h
 * @brief Header file for random.c
 */
#ifndef RANDOM_H
#define RANDOM_H

#if defined(RANDOM_MKL)

#include <mkl_vsl.h>

/* Use Mersenne Twister as default RNG in MKL */
#ifndef RANDOM_MKL_RNG
#define RANDOM_MKL_RNG VSL_BRNG_SFMT19937
#endif

typedef struct {
    VSLStreamStatePtr r;
} random_data;

void random_mkl_init(random_data* rdata, int seed);
double random_mkl_uniform(random_data* rdata);
double random_mkl_normal(random_data* rdata);
void random_mkl_uniform_simd(random_data* rdata, int n, double* r);
void random_mkl_normal_simd(random_data* rdata, int n, double* r);

#define random_init(data, seed) random_mkl_init(data, seed)
#define random_uniform(data) random_mkl_uniform(data)
#define random_normal(data) random_mkl_normal(data)
#define random_uniform_simd(data, n, r) random_mkl_uniform_simd(data, n, r)
#define random_normal_simd(data, n, r) random_mkl_normal_simd(data, n, r)


#elif defined(RANDOM_GSL)

#include <gsl/gsl_rng.h>

typedef struct {
    gsl_rng* r;
} random_data;

void random_gsl_init(random_data* rdata, int seed);
double random_gsl_uniform(random_data* rdata);
double random_gsl_normal(random_data* rdata);
void random_gsl_uniform_simd(random_data* rdata, int n, double* r);
void random_gsl_normal_simd(random_data* rdata, int n, double* r);

#define random_init(data, seed) random_gsl_init(data, seed)
#define random_uniform(data) random_gsl_uniform(data)
#define random_normal(data) random_gsl_normal(data)
#define random_uniform_simd(data, n, r) random_gsl_uniform_simd(data, n, r)
#define random_normal_simd(data, n, r) random_gsl_normal_simd(data, n, r)


#else /* No RNG lib defined, use drand48 */

//#define _XOPEN_SOURCE 500
#include <stdlib.h>

#pragma omp declare target
typedef void* random_data;

double random_drand48_normal();
void random_drand48_uniform_simd(int n, double* r);
void random_drand48_normal_simd(int n, double* r);

//#define random_init(data, seed) srand48(seed)
#define random_init(data, seed) 
//#define random_uniform(data) drand48()
#define random_uniform(data) 1.
#define random_normal(data) random_drand_normal()
#define random_uniform_simd(data, n, r) random_drand48_uniform_simd(n, r)
#define random_normal_simd(data, n, r) random_drand48_normal_simd(n, r)

#pragma omp end declare target

#endif

#endif
