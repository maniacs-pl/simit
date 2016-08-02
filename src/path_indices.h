#ifndef SIMIT_PATH_INDICES_H
#define SIMIT_PATH_INDICES_H

#include <ostream>
#include <map>
#include <memory>
#include <typeinfo>

#include "graph.h"
#include "path_expressions.h"
#include "interfaces/printable.h"

namespace simit {
class Set;
namespace pe {
class PathExpression;
class PathIndexBuilder;
class PathIndexImpl;

class PathIndexImpl : public interfaces::Printable {
public:
  class ElementIterator {
  public:
    ElementIterator(unsigned currElem) : currElem(currElem) {}
    ElementIterator(const ElementIterator& it) : currElem(it.currElem) {}
    ElementIterator& operator++() {++currElem; return *this;}

    friend bool operator==(const ElementIterator& lhs,
                           const ElementIterator& rhs) {
      return lhs.currElem == rhs.currElem;
    }

    friend bool operator!=(const ElementIterator& lhs,
                           const ElementIterator& rhs) {
      return lhs.currElem != rhs.currElem;
    }

    const unsigned& operator*() const {return currElem;}

  private:
    unsigned currElem;
  };

  class Neighbors {
  public:
    class Iterator {
    public:
      class Base {
      public:
        Base() {}
        virtual ~Base() {}
        virtual void operator++() = 0;
        virtual unsigned operator*() const = 0;
        virtual Base* clone() const = 0;
        friend bool operator==(const Base &l, const Base &r) {
          return typeid(l) == typeid(r) && l.eq(r);
        }
      protected:
        virtual bool eq(const Base &o) const = 0;
      };

    Iterator() : impl(nullptr) {}
    Iterator(Base *impl) : impl(impl) {}
    Iterator(const Iterator& o) : impl(o.impl->clone()) {}
    ~Iterator() {delete impl;}
    Iterator& operator=(const Iterator& o) {
      if (impl != o.impl) { delete impl; impl = o.impl->clone(); }
      return *this;
    }

    Iterator& operator++() {++(*impl); return *this;}
    unsigned operator*() const {return *(*impl);}
    bool operator==(const Iterator& o) const {
      return (impl == o.impl) || (*impl == *o.impl);
    }
    bool operator!=(const Iterator& o) const {return !(*this == o);}
      
    private:
      Base *impl;
    };

    class Base {
    public:
      virtual Iterator begin() const = 0;
      virtual Iterator end() const = 0;
    };

    Neighbors() : impl(nullptr) {}
    Neighbors(Base *impl) : impl(impl) {}

    Iterator begin() const {return impl->begin();}
    Iterator end() const {return impl->end();}

  private:
    Base *impl;
  };

  virtual ~PathIndexImpl() {}

  virtual unsigned numElements() const = 0;
  virtual unsigned numNeighbors(unsigned elemID) const = 0;
  virtual unsigned numNeighbors() const = 0;

  ElementIterator begin() const {return ElementIterator(0);}
  ElementIterator end() const {return ElementIterator(numElements());}

  virtual Neighbors neighbors(unsigned elemID) const = 0;

private:
  mutable long ref = 0;
  friend inline void aquire(PathIndexImpl *p) {++p->ref;}
  friend inline void release(PathIndexImpl *p) {if (--p->ref==0) delete p;}
};


/// A Path Index enumerates the neighbors of an element through all the paths
/// described by a path expression.
class PathIndex : public util::IntrusivePtr<PathIndexImpl> {
public:
  typedef PathIndexImpl::ElementIterator ElementIterator;
  typedef PathIndexImpl::Neighbors Neighbors;

  PathIndex() : IntrusivePtr(nullptr) {}

  /// The number of elements that this path index maps to their neighbors.
  unsigned numElements() const {return ptr->numElements();}

  /// The sum of number of neighbors of each element covered by this path index.
  unsigned numNeighbors() const {return ptr->numNeighbors();}

  /// The number of path neighbors of `elem`.
  unsigned numNeighbors(unsigned elemID) const {
    return ptr->numNeighbors(elemID);
  }

  /// Iterator that iterates over the elements covered by this path index.
  ElementIterator begin() const {return ptr->begin();}
  ElementIterator end() const {return ptr->end();}

  /// Get the neighbors of `elem` through this path index.
  Neighbors neighbors(unsigned elemID) const {
    return ptr->neighbors(elemID);
  }

  friend std::ostream &operator<<(std::ostream&, const PathIndex&);

private:
  /// PathIndex objects are constructed through a PathIndexBuilder.
  PathIndex(PathIndexImpl *impl) : IntrusivePtr(impl) {}
  friend PathIndexBuilder;
};


/// A SetEndpointPathIndex uses a Set's endpoint list to find path neighbors.
class SetEndpointPathIndex : public PathIndexImpl {
public:
  unsigned numElements() const;
  unsigned numNeighbors(unsigned elemID) const;
  unsigned numNeighbors() const;

  Neighbors neighbors(unsigned elemID) const;

private:
  const simit::Set &edgeSet;

  void print(std::ostream &os) const;

  friend PathIndexBuilder;
  SetEndpointPathIndex(const simit::Set &edgeSet);
};


/// In a SegmentedPathIndex the path neighbors are packed into a segmented
/// vector with no holes. This is equivalent to CSR indices.
class SegmentedPathIndex : public PathIndexImpl {
public:
  ~SegmentedPathIndex() {
    free(coordsData);
    free(sinksData);
  }

  unsigned numElements() const {return numElems;}
  unsigned numNeighbors() const {return coordsData[numElems];}

  const unsigned* getCoordData() const {return coordsData;}
  const unsigned* getSinkData() const {return sinksData;}

  unsigned numNeighbors(unsigned elemID) const {
    iassert(numElems > elemID);
    return coordsData[elemID+1]-coordsData[elemID];
  }

  Neighbors neighbors(unsigned elemID) const;

private:
  /// Segmented vector, where `coordsData[i]:coordsData[i+1]` is the range of
  /// locations of neighbors of `i` in `sinksData`.
  size_t numElems;
  uint32_t* coordsData;
  uint32_t* sinksData;

  void print(std::ostream &os) const;

  friend PathIndexBuilder;

  SegmentedPathIndex(size_t numElements, uint32_t *nbrsStart, uint32_t *nbrs)
      : numElems(numElements), coordsData(nbrsStart), sinksData(nbrs) {}

  SegmentedPathIndex() : numElems(0), coordsData(nullptr), sinksData(nullptr) {
    coordsData = new uint32_t[1];
    coordsData[0] = 0;
  }
};

template <typename PI>
inline bool isa(PathIndex pi) {
  return pi.defined() && dynamic_cast<const PI*>(pi.ptr) != nullptr;
}

template <typename PI>
inline const PI* to(PathIndex pi) {
  iassert(isa<PI>(pi)) << "Wrong PathIndex type " << pi;
  return static_cast<const PI*>(pi.ptr);
}


/// A builder that builds path indices by evaluating path expressions on graphs.
/// The builder memoizes previously computed path indices, and uses these to
/// accelerate subsequent path index construction (since path expressions can be
/// recursively constructed from path expressions).
class PathIndexBuilder {
public:
  PathIndexBuilder() {}
  PathIndexBuilder(std::map<std::string, const simit::Set*> bindings)
      : bindings(bindings) {}

  // Build a Segmented path index by evaluating the `pe` over the given graph.
  PathIndex buildSegmented(const PathExpression &pe, unsigned sourceEndpoint);

  void bind(std::string name, const simit::Set* set);

  const simit::Set* getBinding(pe::Set pset) const;
  const simit::Set* getBinding(ir::Var var) const;

private:
  std::map<std::pair<PathExpression,unsigned>, PathIndex> pathIndices;
  std::map<std::string, const simit::Set*> bindings;
};

}}

#endif
