#include "runtime.h"

#include <cmath>

#include "timers.h"
#include "stdio.h"
#include <chrono>

#ifdef EIGEN
#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <Eigen/IterativeLinearSolvers>
#endif

extern "C" {
int loc(int v0, int v1, int *neighbors_start, int *neighbors) {
  int l = neighbors_start[v0];
  while(neighbors[l] != v1) l++;
  return l;
}

double atan2_f64(double y, double x) {
  return atan2(y, x);
}

float atan2_f32(float y, float x) {
  double d_y = y;
  double d_x = x;
  return (float)atan2(d_y, d_x);
}

double tan_f64(double x) {
  return tan(x);
}

float tan_f32(float x) {
  double d_x = x;
  return (float)tan(d_x);
}

double asin_f64(double x) {
  return asin(x);
}

float asin_f32(float x) {
  double d_x = x;
  return (float)asin(d_x);
}

double acos_f64(double x) {
  return acos(x);
}

float acos_f32(float x) {
  double d_x = x;
  return (float)acos(d_x);
}

double det3_f64(double * a){
  return a[0] * (a[4]*a[8]-a[5]*a[7])
       - a[1] * (a[3]*a[8]-a[5]*a[6])
       + a[2] * (a[3]*a[7]-a[4]*a[6]);
}

float det3_f32(float * a){
  return a[0] * (a[4]*a[8]-a[5]*a[7])
       - a[1] * (a[3]*a[8]-a[5]*a[6])
       + a[2] * (a[3]*a[7]-a[4]*a[6]);
}

void inv3_f64(double * a, double * inv){
  double cof00 = a[4]*a[8]-a[5]*a[7];
  double cof01 =-a[3]*a[8]+a[5]*a[6];
  double cof02 = a[3]*a[7]-a[4]*a[6];

  double cof10 =-a[1]*a[8]+a[2]*a[7];
  double cof11 = a[0]*a[8]-a[2]*a[6];
  double cof12 =-a[0]*a[7]+a[1]*a[6];

  double cof20 = a[1]*a[5]-a[2]*a[4];
  double cof21 =-a[0]*a[5]+a[2]*a[3];
  double cof22 = a[0]*a[4]-a[1]*a[3];

  double determ = a[0] * cof00 + a[1] * cof01 + a[2]*cof02;

  determ = 1.0/determ;
  inv[0] = cof00 * determ;
  inv[1] = cof10 * determ;
  inv[2] = cof20 * determ;

  inv[3] = cof01 * determ;
  inv[4] = cof11 * determ;
  inv[5] = cof21 * determ;

  inv[6] = cof02 * determ;
  inv[7] = cof12 * determ;
  inv[8] = cof22 * determ;
}

void inv3_f32(float * a, float * inv){
  float cof00 = a[4]*a[8]-a[5]*a[7];
  float cof01 =-a[3]*a[8]+a[5]*a[6];
  float cof02 = a[3]*a[7]-a[4]*a[6];

  float cof10 =-a[1]*a[8]+a[2]*a[7];
  float cof11 = a[0]*a[8]-a[2]*a[6];
  float cof12 =-a[0]*a[7]+a[1]*a[6];

  float cof20 = a[1]*a[5]-a[2]*a[4];
  float cof21 =-a[0]*a[5]+a[2]*a[3];
  float cof22 = a[0]*a[4]-a[1]*a[3];

  float determ = a[0] * cof00 + a[1] * cof01 + a[2]*cof02;

  determ = 1.0/determ;
  inv[0] = cof00 * determ;
  inv[1] = cof10 * determ;
  inv[2] = cof20 * determ;

  inv[3] = cof01 * determ;
  inv[4] = cof11 * determ;
  inv[5] = cof21 * determ;

  inv[6] = cof02 * determ;
  inv[7] = cof12 * determ;
  inv[8] = cof22 * determ;
}

double complexNorm_f64(double r, double i) {
  return sqrt(r*r+i*i);
}

float complexNorm_f32(float r, float i) {
  return sqrt(r*r+i*i);
}

void simitStoreTime(int i, double value) {
  simit::ir::TimerStorage::getInstance().storeTime(i, value);
}

double simitClock() {
  using namespace std::chrono;
  auto t = high_resolution_clock::now();
  time_point<high_resolution_clock,microseconds> usec = time_point_cast<microseconds>(t);
  return (double)(usec.time_since_epoch().count());
}
} // extern "C"

// Solvers
#define SOLVER_ERROR                                            \
do {                                                            \
  ierror << "Solvers require that Simit was built with Eigen."; \
} while (false)

template <typename Float>
void solve(int n,  int m,  int* rowPtr, int* colIdx,
           int nn, int mm, Float* A, Float* x, Float* b) {
#ifdef EIGEN
  using namespace Eigen;

  auto xvec = new Eigen::Map<Eigen::Matrix<Float,Dynamic,1>>(x, m);
  auto cvec = new Eigen::Map<Eigen::Matrix<Float,Dynamic,1>>(b, n);
  auto mat = csr2eigen<Float, Eigen::ColMajor>(n, m, rowPtr, colIdx, nn, mm, A);

  ConjugateGradient<SparseMatrix<Float>,Lower,IdentityPreconditioner> solver;
  solver.setMaxIterations(50);
  solver.compute(mat);
  *cvec = solver.solve(*xvec);
#else
  SOLVER_ERROR;
#endif
}

extern "C" {
void cMatSolve_f64(int n,  int m,  int* rowPtr, int* colIdx,
                   int nn, int mm, double* A, double* x, double* b) {
  return solve(n, m, rowPtr, colIdx, nn, mm, A, x, b);
}
void cMatSolve_f32(int n,  int m,  int* rowPtr, int* colIdx,
                   int nn, int mm, float* A, float* x, float* b) {
  return solve(n, m, rowPtr, colIdx, nn, mm, A, x, b);
}
}

template <typename Float>
void chol(int Bn,  int Bm,  int* Browptr, int* Bcolidx,
          int Bnn, int Bmm, Float* B,
          int An,  int Am,  int** Arowptr, int** Acolidx,
          int Ann, int Amm, Float** A) {
#ifdef EIGEN
  terror << "chol not implemented yet.";
#else
  SOLVER_ERROR;
#endif
}

extern "C" {
void schol(int Bn,  int Bm,  int* Browptr, int* Bcolidx,
           int Bnn, int Bmm, float* B,
           int An,  int Am,  int** Arowptr, int** Acolidx,
           int Ann, int Amm, float** A) {
  return chol(Bn, Bm, Browptr, Bcolidx, Bnn, Bmm, B,
              An, Am, Arowptr, Acolidx, Ann, Amm, A);
}
void dchol(int Bn,  int Bm,  int* Browptr, int* Bcolidx,
           int Bnn, int Bmm, double* B,
           int An,  int Am,  int** Arowptr, int** Acolidx,
           int Ann, int Amm, double** A) {
  return chol(Bn, Bm, Browptr, Bcolidx, Bnn, Bmm, B,
              An, Am, Arowptr, Acolidx, Ann, Amm, A);
}
}
