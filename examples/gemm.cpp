/** GEMM example for runtime. */
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <random>
#include <cstring>
#include <vector>
#include <mpi.h>

#include "data.hpp"
#include "task.hpp"
#include "dependencies.hpp"
#include "context/context.hpp"

typedef double scalar_t;

/** Sequential stuff */

/** Matrix structure. */
class Matrix {
public:
  scalar_t* mat;
  int m, n;

public:
  static Matrix random(int m, int n) {
    size_t elements = ((size_t) m) * n;
    scalar_t* mat = new scalar_t[elements];
    std::random_device rdev;
    std::mt19937 rgen(rdev());
    std::uniform_real_distribution<scalar_t> dist(0, 1);

    for (size_t i = 0; i < elements; ++i) {
      mat[i] = dist(rgen);
    }
    return (Matrix) {mat, m, n};
  }

  const Matrix& operator=(const Matrix& o) {
    if (mat) {
      delete[] mat;
    }
    const size_t elements = ((size_t) o.m) * o.n;
    mat = new scalar_t[elements];
    memcpy(mat, o.mat, sizeof(scalar_t) * elements);
    n = o.n;
    m = o.m;
    return *this;
  }

  Matrix(int m, int n) : mat(NULL), m(m), n(n) {
    const size_t elements = ((size_t) m) * n;
    mat = new scalar_t[elements];
  }

  Matrix(scalar_t* mat, int m, int n) : mat(mat), m(m), n(n) {}

  Matrix(const Matrix& o) : mat(NULL), m(o.m), n(o.n) {
    const size_t elements = ((size_t) o.m) * o.n;
    mat = new scalar_t[elements];
    memcpy(mat, o.mat, sizeof(scalar_t) * elements);
  }

  ~Matrix() {
    if (mat) {
      delete[] mat;
    }
  }

  bool operator==(const Matrix& o) const {
    if ((m != o.m) || (n != o.n)) {
      return false;
    }
    const size_t elements = ((size_t) m) * n;
    for (size_t i = 0; i < elements; ++i) {
      if (mat[i] != o.mat[i]) {
        return false;
      }
    }
    return true;
  }

  bool almostEquals(const Matrix& o, const scalar_t epsilon) const {
    if ((m != o.m) || (n != o.n)) {
      return false;
    }
    const size_t elements = ((size_t) m) * n;
    for (size_t i = 0; i < elements; ++i) {
      if (fabs(mat[i] - o.mat[i]) > epsilon) {
        return false;
      }
    }
    return true;
  }

  inline scalar_t get(int i, int j) const {
    return mat[i + j * n];
  }
  inline scalar_t& get(int i, int j) {
    return mat[i + j * n];
  }

  void scale(const scalar_t alpha) {
    DECLARE_CONTEXT;
    if (alpha == 1.) {
      return;
    }
    const size_t elements = ((size_t) m) * n;
    if (alpha == 0.) {
      memset(mat, 0, elements * sizeof(scalar_t));
    } else {
      for (size_t i = 0; i < elements; ++i) {
        mat[i] *= alpha;
      }
    }
  }

  // this <- alpha.a.b + beta.this
  void gemm(const scalar_t alpha, const Matrix& a, const Matrix& b, const scalar_t beta) {
    DECLARE_CONTEXT;
    Matrix& c = *this;
    assert(a.mat != c.mat);
    assert(b.mat != c.mat);
    c.scale(beta); // invalid if c is the same as a or b
    assert(a.n == b.m);
    assert(a.m == c.m);
    assert(b.n == c.n);
    for (int i = 0; i < c.m; ++i) {
      for (int j = 0; j < c.n; ++j) {
        for (int k = 0; k < a.n; ++k) {
          c.get(i, j) += alpha * a.get(i, k) * b.get(k, j);
        }
      }
    }
  }

  void toTile(int ib, int jb, Matrix& tile) const {
    const int iOffset = ib * tile.m;
    const int jOffset = jb * tile.n;
    for (int j = 0; j < tile.n; j++) {
      for (int i = 0; i < tile.m; i++) {
        tile.get(i, j) = get(i + iOffset, j + jOffset);
      }
    }
  }

  void fromTile(int ib, int jb, const Matrix& tile) {
    const int iOffset = ib * tile.m;
    const int jOffset = jb * tile.n;
    for (int j = 0; j < tile.n; j++) {
      for (int i = 0; i < tile.m; i++) {
        get(i + iOffset, j + jOffset) = tile.get(i, j);
      }
    }
  }
};


Matrix** toTiles(const Matrix& m, const int nTiles) {
  int mPerTile = m.m / nTiles;
  int nPerTile = m.n / nTiles;
  Matrix** tiles = new Matrix*[nTiles * nTiles];
  for (int j = 0; j < nTiles; j++) {
    for (int i = 0; i < nTiles; i++) {
      tiles[i + j * nTiles] = new Matrix(mPerTile, nPerTile);
      m.toTile(i, j, *tiles[i + j * nTiles]);
    }
  }
  return tiles;
}

void fromTiles(Matrix& m, Matrix** tiles, const int nTiles) {
  for (int j = 0; j < nTiles; j++) {
    for (int i = 0; i < nTiles; i++) {
      m.fromTile(i, j, *tiles[i + j * nTiles]);
    }
  }
}

// alpha a.b + beta c -> c
void tiledGemm(Matrix** c, scalar_t alpha, Matrix** a, Matrix** b, scalar_t beta,
               int nTiles) {
  for (int j = 0; j < nTiles; j++) {
    for (int i = 0; i < nTiles; i++) {
      Matrix* c_ij = c[i + j * nTiles];
      c_ij->scale(beta);
      for (int k = 0; k < nTiles; k++) {
        c_ij->gemm(alpha, *a[i + k * nTiles], *b[k + j * nTiles], 1.);
      }
    }
  }
}

void deleteTiles(Matrix** tiles, int nTiles) {
  for (int i = 0; i < nTiles * nTiles; ++i) {
    delete tiles[i];
  }
  delete tiles;
}


/** Runtime stuff */

/** Very simple data class that doesn't work wth MPI and OOC. */
class SimpleData : public Data {
public:
  SimpleData() : Data() {}
  ssize_t pack(void** ptr) {
    return 0;
  }
  void unpack(void* ptr, ssize_t count) {}
  void deallocate() {}
  size_t size() {return 0;}
};

class MatrixData : public SimpleData {
private:
  Matrix* m;
public:
  MatrixData(Matrix* m) : SimpleData(), m(m) {}
};


class ScaleTask : public Task {
private:
  scalar_t alpha;
  Matrix& m;

public:
  ScaleTask(const scalar_t alpha, Matrix& m) : Task("scale"), alpha(alpha), m(m) {}
  void call() {
    m.scale(alpha);
  }
};


class GemmTask : public Task {
private:
  scalar_t alpha, beta;
  Matrix& c;
  const Matrix& a;
  const Matrix& b;

public:
  GemmTask(Matrix& c, scalar_t alpha, const Matrix& a, const Matrix& b, scalar_t beta)
    : Task("gemm"), alpha(alpha), beta(beta), c(c), a(a), b(b) {}
  void call() {
    c.gemm(alpha, a, b, beta);
  }
};

void runtimeGemm(Matrix** c, scalar_t alpha, Matrix** a, Matrix** b,
                 scalar_t beta, int nTiles)  {
  DECLARE_CONTEXT;
  int nnTiles = nTiles * nTiles;
  std::vector<MatrixData> cData(c, c + nnTiles), aData(a, a + nnTiles),
    bData(b, b + nnTiles);
  TaskScheduler& s = TaskScheduler::getInstance();
  s.setMpiComm(MPI_COMM_WORLD);
  for (int j = 0; j < nTiles; j++) {
    for (int i = 0; i < nTiles; i++) {
      Matrix& c_ij = *c[i + j * nTiles];
      s.insertTask(new ScaleTask(beta, c_ij), {{&cData[i + j * nTiles], toyRT_WRITE}});
      for (int k = 0; k < nTiles; k++) {
        Matrix& a_ik = *a[i + k * nTiles];
        Matrix& b_kj = *b[k + j * nTiles];
        s.insertTask(new GemmTask(c_ij, alpha, a_ik, b_kj, 1.),
                     {{&cData[i + j * nTiles], toyRT_WRITE}, {&aData[i + k * nTiles], toyRT_READ}, {&bData[k + j * nTiles], toyRT_READ}});
      }
    }
  }
  s.go(4); // 4 threads
}


int main(int argc, char** argv) {
  {
    DECLARE_CONTEXT;
    tracing_set_worker_index_func(toyrtWorkerId);

  MPI_Init(&argc, &argv);
  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " N ntiles" << std::endl;
    return 0;
  }
  int n = atoi(argv[1]);
  int nTiles = atoi(argv[2]);
  assert(n % nTiles == 0);

  std::cout << "Creating random a... "<< std::endl;
  Matrix a = Matrix::random(n, n);
  std::cout << "Creating random b... "<< std::endl;
  Matrix b = Matrix::random(n, n);
  std::cout << "Creating c... "<< std::endl;
  Matrix c(n, n);
  std::cout << "Creating c2... "<< std::endl;
  Matrix c2(n, n);

  std::cout << "Computing a.b -> c sequentially "<< std::endl;
  c.gemm(1, a, b, 0);

  std::cout << "Splitting a into tiles... "<< std::endl;
  Matrix** aTiles = toTiles(a, nTiles);
  std::cout << "Splitting b into tiles... "<< std::endl;
  Matrix** bTiles = toTiles(b, nTiles);
  std::cout << "Splitting c2 into tiles... "<< std::endl;
  Matrix** cTiles = toTiles(c2, nTiles);
  std::cout << "Computing a.b -> c2 in parallel "<< std::endl;
  runtimeGemm(cTiles, 1, aTiles, bTiles, 0, nTiles);
  // tiledGemm(cTiles, 1, aTiles, bTiles, 0, nTiles);
  deleteTiles(aTiles, nTiles);
  deleteTiles(bTiles, nTiles);
  std::cout << "Gathering c2 from tiles... "<< std::endl;
  fromTiles(c2, cTiles, nTiles);
  deleteTiles(cTiles, nTiles);

  std::cout << "Checking result... "<< std::endl;
  assert(c.almostEquals(c2, 1e-15));
  assert(c == c2);
  }
  tracing_dump("gemm_trace.json");

  return 0;
}
