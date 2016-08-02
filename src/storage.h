#ifndef SIMIT_STORAGE_H
#define SIMIT_STORAGE_H

#include <map>
#include <memory>
#include <string>

#include "intrusive_ptr.h"
#include "stencils.h"

namespace simit {
namespace pe {
class PathExpression;
}
namespace ir {
class Func;
class Var;
class Stmt;
class Expr;
class Environment;
class TensorIndex;


/// The storage descriptor of a tensor. Tensors can be dense, diagonal or
/// indexed (BCSR).  Indexed tensor descriptors stores a tensor index object
/// that describes the index.
class TensorStorage {
public:
  enum Kind {
    /// Undefined storage.
    Undefined,

    /// The dense tensor stored row major.
    Dense,

    /// A diagonal matrix.
    Diagonal,

    /// A sparse matrix whose non-zero components are accessible through a
    /// tensor index.
    Indexed,

    /// A sparse matrix, whose non-zeros components are accessible through a
    /// *computable* tensor index (i.e. no memory-based structures).
    /// Stencil-assembled matrices can be stored this way, since all
    /// non-zeros appear on a fixed number of diagonals determined when
    /// compiling the assembly function.
    Stencil,
  };

  /// Create an undefined tensor storage.
  TensorStorage();

  /// Create a tensor storage descriptor.
  TensorStorage(Kind kind);

  /// Create an indexed tensor storage descriptor with an index.
  TensorStorage(Kind kind, const TensorIndex& index);

  /// Retrieve the tensor storage type.
  Kind getKind() const;

  /// True if the storage descriptor has a tensor index, false otherwise.
  bool hasTensorIndex() const;

  /// Return the storage descriptor's tensor index.  Assumes the tensor is an
  /// indexed tensor.
  const TensorIndex& getTensorIndex() const;
  TensorIndex& getTensorIndex();

  /// Set the storage descriptor's tensor index.
  void setTensorIndex(Var tensor);

private:
  struct Content;
  std::shared_ptr<Content> content;
};
std::ostream &operator<<(std::ostream&, const TensorStorage&);


/// The storage of a set of tensors.
class Storage {
public:
  Storage();

  /// Add storage for a tensor variable.
  void add(const Var &tensor, TensorStorage tensorStorage);

  /// Add the variables from the `other` storage to this storage.
  void add(const Storage &other);

  /// True if the tensor has a storage descriptor, false otherwise.
  bool hasStorage(const Var &tensor) const;

  /// Retrieve the storage of a tensor variable to modify it.
  TensorStorage &getStorage(const Var &tensor);

  /// Retrieve the storage of a tensor variable to inspect it.
  const TensorStorage &getStorage(const Var &tensor) const;

  /// Iterator over storage Vars in this Storage descriptor.
  class Iterator {
  public:
    struct Content;
    Iterator(Content *content);
    ~Iterator();
    const Var &operator*();
    const Var *operator->();
    Iterator& operator++();
    friend bool operator!=(const Iterator&, const Iterator&);
  private:
    Content *content;
  };

  /// Get an iterator pointing to the first Var in this Storage.
  Iterator begin() const;

  /// Get an iterator pointing to the last Var in this Storage.
  Iterator end() const;

private:
  struct Content;
  std::shared_ptr<Content> content;
};
std::ostream &operator<<(std::ostream&, const Storage&);

/// Adds storage descriptors for each tensor in `func` not already described.
void updateStorage(const Func &func, Storage *storage, Environment* env);

/// Adds storage descriptors for each tensor in `stmt` not already described.
void updateStorage(const Stmt &stmt, Storage *storage, Environment* env);
}}

#endif
