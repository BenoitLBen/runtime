/** GEMM example for runtime. */
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

#include <mpi.h>

#include "context/context.hpp"
#include "data.hpp"
#include "dependencies.hpp"
#include "task.hpp"

typedef double scalar_t;

/** Sequential stuff */

/** Matrix structure. */
class Matrix {
public:
  static Matrix random(int rows, int cols) {
    size_t elements = static_cast<size_t>(rows) * cols;
    auto mat = std::unique_ptr<scalar_t[]>(new scalar_t[elements]);
    std::random_device rdev;
    std::mt19937 rgen(rdev());
    std::uniform_real_distribution<scalar_t> dist(0, 1);

    for (size_t i = 0; i < elements; ++i) {
      mat.get()[i] = dist(rgen);
    }
    return {std::move(mat), rows, cols};
  }

  const Matrix& operator=(const Matrix& o) {
    assert(rows_ == o.rows_);
    assert(cols_ == o.cols_);
    size_t elements = o.rows() * o.cols();
    mat_ = std::unique_ptr<scalar_t[]>(new scalar_t[elements]);
    memcpy(mat_.get(), o.mat_.get(), sizeof(scalar_t) * elements);
    return *this;
  }

  Matrix(int rows, int cols) : mat_(nullptr), rows_(rows), cols_(cols) {
    size_t elements = this->rows() * this->cols();
    mat_ = std::unique_ptr<scalar_t[]>(new scalar_t[elements]);
  }

  Matrix(std::unique_ptr<scalar_t[]> mat, int rows, int cols)
    : mat_(std::move(mat)), rows_(rows), cols_(cols) {}

  Matrix(const Matrix& o) : mat_(nullptr), rows_(o.rows_), cols_(o.cols_) {
    size_t elements = o.rows() * o.cols();
    mat_ = std::unique_ptr<scalar_t[]>(new scalar_t[elements]);
    memcpy(mat_.get(), o.mat_.get(), sizeof(scalar_t) * elements);
  }

  bool operator==(const Matrix& o) const {
    if ((rows_ != o.rows_) || (cols_ != o.cols_)) {
      return false;
    }
    size_t elements = rows() * cols();
    bool is_equal = !memcmp(mat_.get(), o.mat_.get(),
                            sizeof(scalar_t) * elements);
    return is_equal;
  }

  bool almostEquals(const Matrix& o, const scalar_t epsilon) const {
    if ((rows_ != o.rows_) || (cols_ != o.cols_)) {
      return false;
    }
    const size_t elements = rows() * cols();
    for (size_t i = 0; i < elements; ++i) {
      if (fabs(mat_.get()[i] - o.mat_.get()[i]) > epsilon) {
        return false;
      }
    }
    return true;
  }

  scalar_t get(int i, int j) const {
    return mat_.get()[i + j * rows_];
  }
  scalar_t& get(int i, int j) {
    return mat_.get()[i + j * rows_];
  }
  size_t rows() const { return rows_; }
  size_t cols() const { return cols_; }

  void scale(const scalar_t alpha) {
    DECLARE_CONTEXT;
    if (alpha == 1.) {
      return;
    }
    const size_t elements = rows() * cols();
    if (alpha == 0.) {
      memset(mat_.get(), 0, elements * sizeof(scalar_t));
    } else {
      for (size_t i = 0; i < elements; ++i) {
        mat_.get()[i] *= alpha;
      }
    }
  }

  // this <- alpha.a.b + beta.this
  void gemm(const scalar_t alpha, const Matrix& a, const Matrix& b,
            const scalar_t beta) {
    DECLARE_CONTEXT;
    Matrix& c = *this;
    assert(a.mat_ != c.mat_);
    assert(b.mat_ != c.mat_);
    c.scale(beta); // invalid if c is the same as a or b
    assert(a.cols_ == b.rows_);
    assert(a.rows_== c.cols_);
    assert(b.cols_ == c.cols_);
    for (int i = 0; i < c.rows_; ++i) {
      for (int j = 0; j < c.cols_; ++j) {
        for (int k = 0; k < a.cols_; ++k) {
          c.get(i, j) += alpha * a.get(i, k) * b.get(k, j);
        }
      }
    }
  }

  void toTile(int ib, int jb, Matrix& tile) const {
    const int iOffset = ib * tile.rows_;
    const int jOffset = jb * tile.cols_;
    for (int j = 0; j < tile.cols_; j++) {
      for (int i = 0; i < tile.rows_; i++) {
        tile.get(i, j) = get(i + iOffset, j + jOffset);
      }
    }
  }

  void fromTile(int ib, int jb, const Matrix& tile) {
    const int iOffset = ib * tile.rows();
    const int jOffset = jb * tile.cols();
    for (int j = 0; j < tile.cols_; j++) {
      for (int i = 0; i < tile.rows_; i++) {
        get(i + iOffset, j + jOffset) = tile.get(i, j);
      }
    }
  }

  scalar_t norm() const {
    scalar_t norm_squared = 0.;
    size_t elements = rows() * cols();
    for (size_t i = 0; i < elements; ++i) {
      scalar_t x = mat_.get()[i];
      norm_squared += x * x;
    }
    return sqrt(norm_squared);
  }

private:
  std::unique_ptr<scalar_t[]> mat_;
  const int rows_;
  const int cols_;
};

using Tiles = std::vector<std::unique_ptr<Matrix>>;

Tiles toTiles(const Matrix& m, const int nTiles) {
  int mPerTile = m.rows() / nTiles;
  int nPerTile = m.cols() / nTiles;
  std::vector<std::unique_ptr<Matrix>> tiles(nTiles * nTiles);
  // Matrix** tiles = new Matrix*[nTiles * nTiles];
  for (int j = 0; j < nTiles; j++) {
    for (int i = 0; i < nTiles; i++) {
      tiles[i + j * nTiles] =
        std::unique_ptr<Matrix>(new Matrix(mPerTile, nPerTile));
      m.toTile(i, j, *tiles[i + j * nTiles]);
    }
  }
  return tiles;
}

void fromTiles(Matrix& m, const Tiles& tiles, const int nTiles) {
  assert(tiles.size() == nTiles * nTiles);
  for (int j = 0; j < nTiles; j++) {
    for (int i = 0; i < nTiles; i++) {
      m.fromTile(i, j, *tiles[i + j * nTiles]);
    }
  }
}

// alpha a.b + beta c -> c
void tiledGemm(Tiles& c, scalar_t alpha, const Tiles& a, const Tiles& b,
               scalar_t beta, int nTiles) {
  for (int j = 0; j < nTiles; j++) {
    for (int i = 0; i < nTiles; i++) {
      Matrix* c_ij = c[i + j * nTiles].get();
      c_ij->scale(beta);
      for (int k = 0; k < nTiles; k++) {
        c_ij->gemm(alpha, *a[i + k * nTiles], *b[k + j * nTiles], 1.);
      }
    }
  }
}

/** Runtime stuff */

/** Very simple data class that doesn't work wth MPI and OOC. */
class SimpleData : public Data {
public:
  SimpleData() : Data() {}
  ssize_t pack(void** ptr) override { return 0; }
  void unpack(void* ptr, ssize_t count) override {}
  void deallocate() override {}
  size_t size() override { return 0; }
};

class MatrixData : public SimpleData {
public:
  MatrixData(const std::unique_ptr<Matrix>& m) : SimpleData(), m(m.get()) {}

private:
  Matrix* m;
};


class ScaleTask : public Task {
private:
  scalar_t alpha;
  Matrix& m;

public:
  ScaleTask(const scalar_t alpha, Matrix& m)
    : Task("scale"), alpha(alpha), m(m) {}
  void call() override {
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
  GemmTask(Matrix& c, scalar_t alpha, const Matrix& a, const Matrix& b,
           scalar_t beta) : Task("gemm"), alpha(alpha), beta(beta), c(c),
                            a(a), b(b) {}
  void call() override {
    c.gemm(alpha, a, b, beta);
  }
};

void runtimeGemm(Tiles& c, scalar_t alpha, const Tiles& a, const Tiles& b,
                 scalar_t beta, int nTiles)  {
  DECLARE_CONTEXT;
  int nnTiles = nTiles * nTiles;
  std::vector<MatrixData> cData(c.begin(), c.begin() + nnTiles);
  std::vector<MatrixData> aData(a.begin(), a.begin() + nnTiles);
  std::vector<MatrixData> bData(b.begin(), b.begin() + nnTiles);
  TaskScheduler& s = TaskScheduler::getInstance();
  s.setMpiComm(MPI_COMM_WORLD);
  for (int j = 0; j < nTiles; j++) {
    for (int i = 0; i < nTiles; i++) {
      Matrix& c_ij = *c[i + j * nTiles];
      s.insertTask(std::unique_ptr<Task>(new ScaleTask(beta, c_ij)),
                   {{&cData[i + j * nTiles], toyRT_WRITE}});
      for (int k = 0; k < nTiles; k++) {
        Matrix& a_ik = *a[i + k * nTiles];
        Matrix& b_kj = *b[k + j * nTiles];
        auto task =
          std::unique_ptr<Task>(new GemmTask(c_ij, alpha, a_ik, b_kj, 1.));
        s.insertTask(std::move(task),
                     {{&cData[i + j * nTiles], toyRT_WRITE},
                      {&aData[i + k * nTiles], toyRT_READ},
                      {&bData[k + j * nTiles], toyRT_READ}});
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
    Tiles aTiles = toTiles(a, nTiles);
    std::cout << "Splitting b into tiles... "<< std::endl;
    Tiles bTiles = toTiles(b, nTiles);
    std::cout << "Splitting c2 into tiles... "<< std::endl;
    Tiles cTiles = toTiles(c2, nTiles);
    std::cout << "Computing a.b -> c2 in parallel "<< std::endl;
    runtimeGemm(cTiles, 1, aTiles, bTiles, 0, nTiles);
    // tiledGemm(cTiles, 1, aTiles, bTiles, 0, nTiles);
    std::cout << "Gathering c2 from tiles... "<< std::endl;
    fromTiles(c2, cTiles, nTiles);

    std::cout << "Checking result... "<< std::endl;
    std::cout << "||C||  = " << c.norm() << "\n"
              << "||C2|| = " << c2.norm() << std::endl;
    assert(c.almostEquals(c2, 1e-15));
    assert(c == c2);
  }
  tracing_dump("gemm_trace.json");

  return 0;
}
