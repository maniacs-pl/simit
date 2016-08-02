#ifndef SIMIT_TENSOR_INDEX_H
#define SIMIT_TENSOR_INDEX_H

#include <string>
#include <ostream>
#include <vector>
#include <memory>

#include "path_expressions.h"
#include "stencils.h"

namespace simit {
namespace pe {
class PathExpression;
}

namespace ir {
class Var;

/// A tensor index is a CSR index that describes the sparsity of a matrix.
/// Tensor indices may have path expressions that describe their sparsity as a
/// function of a Simit graph.  Such tensor indices are added to the
/// environemnt, pre-assembled on function initialization and shared between
/// tensors with the same sparsity.  Tensor indices without path expressions
/// cannot be pre-assembled and must be assembled by the user if they come
/// from an extern function, or by Simit as they are computed.
/// Note: only sparse matrix CSR indices are supported for now.
class TensorIndex {
public:
  enum Kind {PExpr, Sten};
  
  TensorIndex() {}
  TensorIndex(std::string name, pe::PathExpression pexpr);
  TensorIndex(std::string name, StencilLayout stencil);

  /// Get tensor index name
  const std::string getName() const;

  /// Get tensor index kind
  Kind getKind() const;

  /// Get whether the tensor index has computed (vs. stored) row and
  /// coord indices.
  bool isComputed() const;

  /// Get the tensor index's path expression.  Tensor indices with defined path
  /// expressions are stored in the environment, pre-assembled and shared
  /// between tensors with the same sparsity.  Tensors with undefined path
  /// expressions are assembled by the user if they are returned from an extern
  /// function, or by Simit as they are computed.
  const pe::PathExpression& getPathExpression() const;

  /// Return the tensor index's defining stencil.
  const StencilLayout& getStencilLayout() const;

  /// Replace the StencilLayout of the TensorIndex. Used during compile to assign
  /// the index locations of the stencil layout.
  void setStencilLayout(StencilLayout);

  /// Return the tensor index's rowptr array.  A rowptr array contains the
  /// beginning and end of the column indices in the colidx array for each row
  /// of the tensor index.
  /// Note: only sparse matrix CSR indices are supported for now.
  const Var& getRowptrArray() const;

  /// Return the tensor index's colidx array.  A colidx array contains the
  /// column index of every non-zero tensor value.
  /// Note: only sparse matrix CSR indices are supported for now.
  const Var& getColidxArray() const;

  /// Compute the tensor index's rowptr value for a given source.
  const Expr computeRowptr(Expr base) const;

  /// Compute the tensor index's colidx value for a given coord.
  const Expr computeColidx(Expr coord) const;

  /// Defined if the tensor exists, false otherwise.
  bool defined() const {return content != nullptr;}

private:
  struct Content;
  std::shared_ptr<Content> content;
};

std::ostream& operator<<(std::ostream&, const TensorIndex&);

}}

#endif
