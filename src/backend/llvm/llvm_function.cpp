#include "llvm_function.h"

#include <string>
#include <vector>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/DynamicLibrary.h"

#if LLVM_MAJOR_VERSION <= 3 && LLVM_MINOR_VERSION <= 4
#include "llvm/Analysis/Verifier.h"
#else
#include "llvm/IR/Verifier.h"
#endif

#include "llvm_types.h"
#include "llvm_codegen.h"
#include "llvm_data_layouts.h"

#include "backend/actual.h"
#include "graph.h"
#include "graph_indices.h"
#include "tensor_index.h"
#include "path_indices.h"
#include "util/collections.h"
#include "util/util.h"
#include "llvm_util.h"

using namespace std;
using namespace simit::ir;

namespace simit {
namespace backend {

typedef void (*FuncPtrType)();

LLVMFunction::LLVMFunction(ir::Func func, const ir::Storage &storage,
                           llvm::Function* llvmFunc, llvm::Module* module,
                           std::shared_ptr<llvm::EngineBuilder> engineBuilder)
    : Function(func), initialized(false), llvmFunc(llvmFunc), module(module),
      harnessModule(new llvm::Module("simit_harness", LLVM_CTX)),
      storage(storage),
      engineBuilder(engineBuilder),
#if LLVM_MAJOR_VERSION <= 3 && LLVM_MINOR_VERSION <= 5
      executionEngine(engineBuilder->setUseMCJIT(true).create()), // MCJIT EE
#else
      executionEngine(engineBuilder->create()),
#endif
#if LLVM_MAJOR_VERSION <= 3 && LLVM_MINOR_VERSION <= 5
      harnessEngineBuilder(new llvm::EngineBuilder(harnessModule)),
      harnessExecEngine(harnessEngineBuilder->setUseMCJIT(true).create()), // MCJIT EE
#else
      harnessEngineBuilder(new llvm::EngineBuilder(
          unique_ptr<llvm::Module>(harnessModule))),
      harnessExecEngine(harnessEngineBuilder->create()),
#endif
      deinit(nullptr) {

  // Finalize existing module so we can get global pointer hooks
  // from the LLVM memory manager.
  executionEngine->finalizeObject();

  const Environment& env = getEnvironment();

  // Initialize extern pointers
  for (const VarMapping& externMapping : env.getExterns()) {
    Var bindable = externMapping.getVar();

    // Store a pointer to each of the bindable's extern in externPtrs
    vector<void**> extPtrs;
    for (const Var& ext : externMapping.getMappings()) {
      uint64_t addr = executionEngine->getGlobalValueAddress(ext.getName());
      void** extPtr = (void**)addr;
      *extPtr = nullptr;
      extPtrs.push_back(extPtr);
    }
    iassert(!util::contains(this->externPtrs, bindable.getName()));
    this->externPtrs.insert({bindable.getName(), extPtrs});
  }

  // Initialize temporary pointers
  for (const Var& tmp : env.getTemporaries()) {
    iassert(tmp.getType().isTensor())
        << "Only support tensor temporaries";
    // llvm::GlobalValue* llvmTmp = module->getNamedValue(tmp.getName());
    uint64_t addr = executionEngine->getGlobalValueAddress(tmp.getName());
    void** tmpPtr = (void**)addr;
    *tmpPtr = nullptr;
    temporaryPtrs.insert({tmp.getName(), tmpPtr});
  }

  // Initialize tensorIndex ptrs
  for (const TensorIndex& tensorIndex : env.getTensorIndices()) {
    uint64_t addr;

    if (tensorIndex.getKind() == TensorIndex::PExpr) {
      const Var& rowptr = tensorIndex.getRowptrArray();
      addr = executionEngine->getGlobalValueAddress(rowptr.getName());
      const uint32_t** rowptrPtr = (const uint32_t**)addr;
      *rowptrPtr = nullptr;

      const Var& colidx = tensorIndex.getColidxArray();
      addr = executionEngine->getGlobalValueAddress(colidx.getName());
      const uint32_t** colidxPtr = (const uint32_t**)addr;
      *colidxPtr = nullptr;

      const pe::PathExpression& pexpr = tensorIndex.getPathExpression();
      tensorIndexPtrs.insert({pexpr, {rowptrPtr, colidxPtr}});
    }
    else if (tensorIndex.getKind() == TensorIndex::Sten) {
      // No need to build in-memory structures
    }
    else {
      not_supported_yet;
    }
  }

}

LLVMFunction::~LLVMFunction() {
  if (deinit) {
    deinit();
  }
  for (auto& tmpPtr : temporaryPtrs) {
    free(*tmpPtr.second);
    *tmpPtr.second = nullptr;
  }

}

void LLVMFunction::bind(const std::string& name, simit::Set* set) {
  iassert(hasBindable(name));
  iassert(getBindableType(name).isSet());

  if (hasArg(name)) {
    // Check set kinds match
    Type argType = getArgType(name);
    if (argType.isUnstructuredSet()) {
      uassert(set->getKind() == simit::Set::Unstructured)
          << "Must bind an unstructured set to " << name;
    }
    else if (argType.isLatticeLinkSet()) {
      uassert(set->getKind() == simit::Set::LatticeLink)
          << "Must bind a lattice link set to " << name;
    }
    else {
      not_supported_yet;
    }
    arguments[name] = std::unique_ptr<Actual>(new SetActual(set));
    initialized = false;
  }
  else {
    globals[name] = std::unique_ptr<Actual>(new SetActual(set));
    Type globalType = getGlobalType(name);

    if (globalType.isUnstructuredSet()) {
      uassert(set->getKind() == simit::Set::Unstructured)
          << "Must bind an unstructured set to " << name;
    }
    else if (globalType.isLatticeLinkSet()) {
      uassert(set->getKind() == simit::Set::LatticeLink)
          << "Must bind a lattice link set to " << name;
      unsigned ndims = globalType.toLatticeLinkSet()->dimensions;
      vector<int> dimensions = set->getDimensions();
      uassert(dimensions.size() == ndims)
          << "Lattice link set with wrong number of dimensions: "
          << dimensions.size() << " passed, but " << ndims
          << " required";
    }
    else {
      not_supported_yet;
    }

    // Write set values and pointers to the relevant extern
    iassert(util::contains(externPtrs, name) && externPtrs.at(name).size()==1);
    void *externPtr = externPtrs.at(name)[0];
    writeSet(set, globalType, externPtr);
  }
}

void LLVMFunction::bind(const std::string& name, void* data) {
  iassert(hasBindable(name));
  if (hasArg(name)) {
    arguments[name] = std::unique_ptr<Actual>(new TensorActual(data));
    initialized = false;
  }
  else if (hasGlobal(name)) {
    globals[name] = std::unique_ptr<Actual>(new TensorActual(data));
    iassert(util::contains(externPtrs, name) && externPtrs.at(name).size()==1);
    *externPtrs.at(name)[0] = data;
  }
}

void LLVMFunction::bind(const std::string& name, TensorData& tensorData) {
  iassert(hasBindable(name));
  tassert(!hasArg(name)) << "Only support global sparse matrices";

  if (hasGlobal(name)) {
    iassert(util::contains(externPtrs,name))
        << "extern " << util::quote(name) << " does not have any extern ptrs";
    iassert(externPtrs.at(name).size() == 3)
        << "extern " << util::quote(name) << " has wrong size "
        << externPtrs.at(name).size();

    // Sparse matrix externs are ordered: data, rowPtr, colInd
    *externPtrs.at(name)[0] = tensorData.getData();
    *externPtrs.at(name)[1] = (void*)tensorData.getRowPtr();
    *externPtrs.at(name)[2] = (void*)tensorData.getColInd();
  }
}

size_t LLVMFunction::size(const ir::IndexDomain& dimension) {
  size_t result = 1;
  for (const ir::IndexSet& indexSet : dimension.getIndexSets()) {
    switch (indexSet.getKind()) {
      case ir::IndexSet::Range:
        result *= indexSet.getSize();
        break;
      case ir::IndexSet::Set: {
        ir::Expr setExpr = indexSet.getSet();
        iassert(ir::isa<ir::VarExpr>(setExpr))
            << "Attempting to get the static size of a runtime dynamic set: "
            << quote(setExpr);
        string setName = ir::to<ir::VarExpr>(setExpr)->var.getName();

        iassert(util::contains(arguments, setName) ||
                util::contains(globals, setName));
        Actual* setActual = util::contains(arguments, setName)
                            ? arguments.at(setName).get()
                            : globals.at(setName).get();
        iassert(isa<SetActual>(setActual));
        Set* set = to<SetActual>(setActual)->getSet();
        result *= set->getSize();
        break;
      }
      case ir::IndexSet::Single:
      case ir::IndexSet::Dynamic:
        not_supported_yet;
    }
    iassert(result != 0);
  }
  return result;
}

Function::FuncType LLVMFunction::init() {
  pe::PathIndexBuilder piBuilder;

  for (auto& pair : arguments) {
    string name = pair.first;
    Actual* actual = pair.second.get();
    if (isa<SetActual>(actual)) {
      Set* set = to<SetActual>(actual)->getSet();
      piBuilder.bind(name,set);
    }
  }

  const Environment& environment = getEnvironment();

  // Initialize indices
  initIndices(piBuilder, environment);

  // Initialize temporaries
  for (const Var& tmp : environment.getTemporaries()) {
    iassert(util::contains(temporaryPtrs, tmp.getName()));
    const Type& type = tmp.getType();

    if (type.isTensor()) {
      const ir::TensorType* tensorType = type.toTensor();
      unsigned order = tensorType->order();
      iassert(order <= 2) << "Higher-order tensors not supported";

      if (order == 1) {
        // Vectors are currently always dense
        IndexDomain vecDimension = tensorType->getDimensions()[0];
        Type blockType = tensorType->getBlockType();
        size_t blockSize = blockType.toTensor()->size();
        size_t componentSize = tensorType->getComponentType().bytes();
        *temporaryPtrs.at(tmp.getName()) =
            calloc(size(vecDimension) *blockSize, componentSize);
      }
      else if (order == 2) {
        Type blockType = tensorType->getBlockType();
        size_t blockSize = blockType.toTensor()->size();
        size_t componentSize = tensorType->getComponentType().bytes();
        iassert(environment.hasTensorIndex(tmp))
          << "No tensor index for: " << tmp;
        const TensorIndex& ti = environment.getTensorIndex(tmp);
        if (ti.getKind() == TensorIndex::PExpr) {
          const pe::PathExpression& pexpr = ti.getPathExpression();
          iassert(util::contains(pathIndices, pexpr));
          size_t matSize = pathIndices.at(pexpr).numNeighbors() *
              blockSize * componentSize;
          *temporaryPtrs.at(tmp.getName()) = malloc(matSize);
        }
        else if (ti.getKind() == TensorIndex::Sten) {
          auto iss = tensorType->getOuterDimensions();
          iassert(iss.size() == 2);
          iassert(iss[0] == iss[1])
              << "Stencil tensor index must be for a homogeneous matrix";
          size_t latticeSize = size(iss[0]);
          const StencilLayout& stencil = ti.getStencilLayout();
          size_t matSize = stencil.getLayout().size() *
              latticeSize * blockSize * componentSize;
          *temporaryPtrs.at(tmp.getName()) = malloc(matSize);
        }
        else {
          not_supported_yet;
        }
      }
    }
    else {
      unreachable << "don't know how to initialize temporary "
                  << util::quote(tmp);
    }
  }

  // Compile a harness void function without arguments that calls the simit
  // llvm function with pointers to the arguments.
  Function::FuncType func;
  initialized = true;
  vector<string> formals = getArgs();
  iassert(formals.size() == llvmFunc->getArgumentList().size());
  if (llvmFunc->getArgumentList().size() == 0) {
    llvm::Function *initFunc = getInitFunc();
    llvm::Function *deinitFunc = getDeinitFunc();
    uint64_t addr = executionEngine->getFunctionAddress(initFunc->getName());
    FuncPtrType init = reinterpret_cast<decltype(init)>(addr);
    init();
    addr = executionEngine->getFunctionAddress(deinitFunc->getName());
    FuncPtrType deinitPtr = reinterpret_cast<decltype(deinitPtr)>(addr);
    deinit = deinitPtr;
    addr = executionEngine->getFunctionAddress(llvmFunc->getName());
    FuncPtrType funcPtr = reinterpret_cast<decltype(funcPtr)>(addr);
    func = funcPtr;
  }
  else {
    llvm::SmallVector<llvm::Value*, 8> args;
    auto llvmArgIt = llvmFunc->getArgumentList().begin();
    for (const std::string& formal : formals) {
      iassert(util::contains(arguments, formal));

      llvm::Argument* llvmFormal = llvmArgIt++;
      Actual* actual = arguments.at(formal).get();
      ir::Type type = getArgType(formal);
      iassert(type.kind() == ir::Type::Set || type.kind() == ir::Type::Tensor);

      class InitActual : public ActualVisitor {
      public:
        llvm::Value* result;
        Type type;
        llvm::Argument* llvmFormal;
        llvm::Value* init(Actual* a, const Type& t, llvm::Argument* f) {
          this->type = t;
          this->llvmFormal = f;
          a->accept(this);
          return result;
        }

        void visit(SetActual* actual) {
          result = makeSet(actual->getSet(), type);
        }

        void visit(TensorActual* actual) {
          const ir::TensorType* tensorType = type.toTensor();
          void* tensorData = actual->getData();
          result = (llvmFormal->getType()->isPointerTy())
                   ? llvmPtr(*tensorType, tensorData)
                   : llvmVal(*tensorType, tensorData);
        }
      };
      llvm::Value* llvmActual = InitActual().init(actual, type, llvmFormal);
      args.push_back(llvmActual);
    }

    const std::string initFuncName = string(llvmFunc->getName())+"_init";
    const std::string deinitFuncName = string(llvmFunc->getName())+"_deinit";
    const std::string funcName = llvmFunc->getName();

    // Calling main module functions from the harness requires the
    // symbols to be loaded into the memory manager ahead of finalization
    llvm::sys::DynamicLibrary::AddSymbol(
        initFuncName,
        (void*) executionEngine->getFunctionAddress(initFuncName));
    llvm::sys::DynamicLibrary::AddSymbol(
        deinitFuncName,
        (void*) executionEngine->getFunctionAddress(deinitFuncName));
    llvm::sys::DynamicLibrary::AddSymbol(
        funcName,
        (void*) executionEngine->getFunctionAddress(funcName));

    // Create Init/deinit function harnesses
    createHarness(initFuncName, args);
    createHarness(deinitFuncName, args);
    createHarness(funcName, args);

    // Finalize harness module
    harnessExecEngine->finalizeObject();

    // Fetch hard addresses from ExecutionEngine
    auto init = getHarnessFunctionAddress(initFuncName);
    init();
    deinit = getHarnessFunctionAddress(deinitFuncName);

    // Compute function
    func = getHarnessFunctionAddress(funcName);
    iassert(!llvm::verifyModule(*module))
        << "LLVM module does not pass verification";
    iassert(!llvm::verifyModule(*harnessModule))
        << "LLVM harness module does not pass verification";
  }
  return func;
}

void LLVMFunction::print(std::ostream &os) const {
  std::string fstr;
  llvm::raw_string_ostream rsos(fstr);
  module->print(rsos, nullptr);
  os << rsos.str();
}

void LLVMFunction::printMachine(std::ostream &os) const {
  // TODO: Make printMachine write to os, instead of stderr
  llvm::TargetMachine *target = engineBuilder->selectTarget();
  target->Options.PrintMachineCode = true;
  llvm::ExecutionEngine *printee(engineBuilder->create(target));
  printee->getFunctionAddress(llvmFunc->getName());
  target->Options.PrintMachineCode = false;
}

void LLVMFunction::initIndices(pe::PathIndexBuilder& piBuilder,
                               const Environment& environment) {
  // Initialize indices
  for (const TensorIndex& tensorIndex : environment.getTensorIndices()) {
    if (tensorIndex.getKind() == TensorIndex::PExpr) {
      pe::PathExpression pexpr = tensorIndex.getPathExpression();
      pe::PathIndex pidx = piBuilder.buildSegmented(pexpr, 0);
      pathIndices.insert({pexpr, pidx});

      pair<const uint32_t**,const uint32_t**> ptrPair = tensorIndexPtrs.at(pexpr);

      if (isa<pe::SegmentedPathIndex>(pidx)) {
        const pe::SegmentedPathIndex* spidx = to<pe::SegmentedPathIndex>(pidx);
        *ptrPair.first = spidx->getCoordData();
        *ptrPair.second = spidx->getSinkData();
      }
      else {
        not_supported_yet << "doesn't know how to initialize this pathindex type";
      }
    }
    else if (tensorIndex.getKind() == TensorIndex::Sten) {
      // No index to initialize
    }
    else {
      not_supported_yet;
    }
  }
}

void LLVMFunction::createHarness(
    const std::string &name,
    const llvm::SmallVector<llvm::Value*,8> &args) {
  // Build prototype in harnass module as an extrnal linkage to the
  // function in the main module
  llvm::Function *llvmFunc = module->getFunction(name);
  std::vector<string> argNames;
  std::vector<llvm::Type*> argTypes;
  for (llvm::Argument &arg : llvmFunc->getArgumentList()) {
    argNames.push_back(arg.getName());
    argTypes.push_back(arg.getType());
  }
  llvm::Function *llvmFuncProto = createPrototypeLLVM(
      name, argNames, argTypes, harnessModule, true);
      
  std::string harnessName = name + "_harness";
  llvm::Function *harness = createPrototype(
      harnessName, {}, {}, harnessModule, true);
  auto entry = llvm::BasicBlock::Create(LLVM_CTX, "entry", harness);
  llvm::CallInst *call = llvm::CallInst::Create(llvmFuncProto, args, "", entry);
  call->setCallingConv(llvmFunc->getCallingConv());
  llvm::ReturnInst::Create(harnessModule->getContext(), entry);
}

LLVMFunction::FuncType
LLVMFunction::getHarnessFunctionAddress(const std::string &name) {
  std::string fullName = name + "_harness";
  uint64_t addr = harnessExecEngine->getFunctionAddress(fullName);
  iassert(addr != 0)
      << "MCJIT prevents modifying the module after ExecutionEngine code "
      << "generation. Ensure all functions are created before fetching "
      << "function addresses.";
  FuncPtrType funcPtr = reinterpret_cast<decltype(funcPtr)>(addr);
  return funcPtr;
}

llvm::Function *LLVMFunction::getInitFunc() const {
  return module->getFunction(string(llvmFunc->getName()) + "_init");
}

llvm::Function *LLVMFunction::getDeinitFunc() const {
  return module->getFunction(string(llvmFunc->getName()) + "_deinit");
}

}} // unnamed namespace
