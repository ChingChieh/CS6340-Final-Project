#include "Graphs/SVFG.h"
#include "SABER/LeakChecker.h"
#include "SABER/SrcSnkDDA.h"
#include "SVF-FE/LLVMUtil.h"
#include "SVF-FE/PAGBuilder.h"
#include "Util/PathCondAllocator.h"
#include "WPA/Andersen.h"
#include "detector.h"
#include <fmt/core.h>
#include <fmt/format.h>
#include <map>
#include <vector>

using namespace SVF;
using namespace llvm;
using namespace std;

static llvm::cl::opt<std::string>
    InputFilename(cl::Positional, llvm::cl::desc("<input bitcode>"),
                  llvm::cl::init("-"));

static llvm::cl::opt<bool> LEAKCHECKER("leak", llvm::cl::init(false),
                                       llvm::cl::desc("Memory Leak Detection"));

/*!
 * An example to query alias results of two LLVM values
 */
AliasResult aliasQuery(PointerAnalysis *pta, Value *v1, Value *v2) {
  return pta->alias(v1, v2);
}

/*!
 * An example to print points-to set of an LLVM value
 */
std::string printPts(PointerAnalysis *pta, Value *val) {

  std::string str;
  raw_string_ostream rawstr(str);

  NodeID pNodeId = pta->getPAG()->getValueNode(val);
  const NodeBS &pts = pta->getPts(pNodeId);
  for (NodeBS::iterator ii = pts.begin(), ie = pts.end(); ii != ie; ii++) {
    rawstr << " " << *ii << " ";
    PAGNode *targetObj = pta->getPAG()->getPAGNode(*ii);
    if (targetObj->hasValue()) {
      rawstr << "(" << *targetObj->getValue() << ")\t ";
    }
  }

  return rawstr.str();
}

/*!
 * An example to query/collect all successor nodes from a ICFGNode (iNode) along
 * control-flow graph (ICFG)
 */
void traverseOnICFG(ICFG *icfg, const Instruction *inst) {
  ICFGNode *iNode = icfg->getBlockICFGNode(inst);
  FIFOWorkList<const ICFGNode *> worklist;
  std::set<const ICFGNode *> visited;
  worklist.push(iNode);

  /// Traverse along VFG
  while (!worklist.empty()) {
    const ICFGNode *vNode = worklist.pop();
    for (ICFGNode::const_iterator it = vNode->OutEdgeBegin(),
                                  eit = vNode->OutEdgeEnd();
         it != eit; ++it) {
      ICFGEdge *edge = *it;
      ICFGNode *succNode = edge->getDstNode();
      if (visited.find(succNode) == visited.end()) {
        visited.insert(succNode);
        worklist.push(succNode);
      }
    }
  }
}

/*!
 * An example to query/collect all the uses of a definition of a value along
 * value-flow graph (VFG)
 */
void traverseOnVFG(const SVFG *vfg, Value *val) {
  PAG *pag = PAG::getPAG();

  PAGNode *pNode = pag->getPAGNode(pag->getValueNode(val));
  const VFGNode *vNode = vfg->getDefSVFGNode(pNode);
  FIFOWorkList<const VFGNode *> worklist;
  std::set<const VFGNode *> visited;
  worklist.push(vNode);

  /// Traverse along VFG
  while (!worklist.empty()) {
    const VFGNode *vNode = worklist.pop();
    for (VFGNode::const_iterator it = vNode->OutEdgeBegin(),
                                 eit = vNode->OutEdgeEnd();
         it != eit; ++it) {
      VFGEdge *edge = *it;
      VFGNode *succNode = edge->getDstNode();
      if (visited.find(succNode) == visited.end()) {
        visited.insert(succNode);
        worklist.push(succNode);
      }
    }
  }

  /// Collect all LLVM Values
  for (std::set<const VFGNode *>::const_iterator it = visited.begin(),
                                                 eit = visited.end();
       it != eit; ++it) {
    const VFGNode *node = *it;
    // SVFUtil::outs() << *node << "\n";
    /// can only query VFGNode involving top-level pointers (starting with % or
    /// @ in LLVM IR)
    // if(!SVFUtil::isa<MRSVFGNode>(node)){
    //    const PAGNode* pNode = vfg->getLHSTopLevPtr(node);
    //    const Value* val = pNode->getValue();
    //}
  }
}

void traverseCallGraph(PTACallGraph *callgraph) {
  // NOTE: include/Graphs/PTACallGraph.h:218:0

  SVFUtil::outs() << "traverse \n";
  callgraph->verifyCallGraph();

  // NOTE: would be empty
  PTACallGraph::CallEdgeMap &cm = callgraph->getIndCallMap();
  for (auto i = cm.begin(); i != cm.end(); i++) {
    SVFUtil::outs() << "first loop \n";
    auto fset = i->second;
    for (auto j = fset.begin(); j != fset.end(); j++) {
      const SVFFunction *fun = *j;
      SVFUtil::outs() << fun->getName().str() << "\n";
    }
  }

  const PTACallGraph::CallInstToCallGraphEdgesMap &cm2 =
      callgraph->getCallInstToCallGraphEdgesMap();
  for (auto i = cm2.begin(); i != cm2.end(); i++) {
    SVFUtil::outs() << "second loop ==============\n";
    // NOTE: include/Graphs/ICFGNode.h:364:0
    const CallBlockNode *cbn = i->first;
    const PTACallGraph::CallGraphEdgeSet ces = i->second;
    const SVFFunction *caller = cbn->getCaller();
    const Instruction *callsite = cbn->getCallSite();

    const DebugLoc &DL = callsite->getDebugLoc();
    if (DL) {
      DIScope *Scope = cast<DIScope>(DL->getScope());
      string fileName = Scope->getFilename().str();
      u32_t line = DL.getLine();
      u32_t col = DL.getCol();
      // SVFUtil::outs() << "DL: " << *DL << "\n";
      SVFUtil::outs() << fmt::format("file: {}, line: {}, col: {}\n", fileName,
                                     line, col);
    }
    const CallBlockNode::ActualParmVFGNodeVec &params = cbn->getActualParms();

    SVFUtil::outs() << "caller: " << caller->getName().str() << "\n";

    PTACallGraph::FunctionSet fset;
    callgraph->getCallees(cbn, fset);
    for (auto j = fset.begin(); j != fset.end(); j++) {
      const SVFFunction *callee = *j;
      u32_t arg_size = callee->arg_size();
      string callee_name = callee->getName().str();
      SVFUtil::outs() << "callee: " << callee_name << "\n";
      if (callee_name != "g_signal_connect_data") {
        continue;
      }
      SVFUtil::outs() << "opcode name: " << callsite->getOpcodeName() << "\n";
      SVFUtil::outs() << "arg_size: " << arg_size << "\n";
      SVFUtil::outs() << "arg_size2: " << params.size() << "\n";
      for (u32_t arg_num = 0; arg_num < arg_size; arg_num++) {
        const Value *arg = callsite->getOperand(arg_num);
        SVFUtil::outs() << "arg_name: " << arg->getName() << "\n";
      }
      int index = 0;
      for (auto &p : params) {
        const Type *t = p->getType();
        // SVFUtil::outs() << p->getValueName() << "\n";
        p->dump();
        if (p->hasValue()) {
          const Value *v = p->getValue();
          SVFUtil::outs() << "get value " << *v << "\n";
          SVFUtil::outs() << "get type " << *t << "\n";
          if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(v)) {
            SVFUtil::outs() << "is constant expr "
                            << "\n";
          } else if (const ConstantArray *CA = dyn_cast<ConstantArray>(v)) {
            SVFUtil::outs() << "is constant array "
                            << "\n";
          } else if (const GetElementPtrInst *GEP =
                         dyn_cast<GetElementPtrInst>(v)) {
            // NOTE: find "clicked"
            SVFUtil::outs() << "is GEP"
                            << "\n";
            const Value *gv = GEP->getPointerOperand();
            SVFUtil::outs() << *gv << "\n";
            // const ConstantDataArray *CA = cast<ConstantDataArray>(
            //     cast<GlobalVariable>(cast<ConstantExpr>(gv)));
            // const ConstantDataArray *CA =
            // cast<ConstantDataArray>(gv->stripPointerCasts());
            if (const GlobalVariable *GV = dyn_cast<GlobalVariable>(gv)) {
              // Do something with GV
              // GV->dump();
              SVFUtil::outs() << "IS_GV" << *GV << "\n";
              if (const ConstantDataArray *CDA =
                      dyn_cast<ConstantDataArray>(GV->getInitializer())) {
                SVFUtil::outs() << "IS_CDA"
                                << "\n";
                string s = CDA->getAsCString().str();
                SVFUtil::outs() << s << "\n";
              }
            }
          } else if (const BitCastInst *BI = dyn_cast<BitCastInst>(v)) {
            SVFUtil::outs() << "IS BITCAST" << *BI << "\n";
            const Value *op0 = (BI->getOperand(0));
            SVFUtil::outs() << "IS BITCAST 0" << *op0 << "\n";
            if (const LoadInst *LI = dyn_cast<LoadInst>(op0)) {
              SVFUtil::outs() << "0 IS LOAD" << *LI << "\n";
              const Value *op0 = (LI->getOperand(0));
              SVFUtil::outs() << "LOAD op 0" << *op0 << "\n";
            }
            // SVFUtil::outs() << "IS BITCAST 1" << BI->getOperand(1) << "\n";
          }
          index++;
        }
        // for (auto it=callsite->op_begin();it!=callsite->op_end();it++){
        //   Value *operand = *it;
        //   SVFUtil::outs() << "arg_name: " << operand->getName() << "\n";
        // }
      }
    }
  }
}

int main(int argc, char **argv) {

  int arg_num = 0;
  char **arg_value = new char *[argc];
  std::vector<std::string> moduleNameVec;
  SVFUtil::processArguments(argc, argv, arg_num, arg_value, moduleNameVec);
  cl::ParseCommandLineOptions(arg_num, arg_value,
                              "Whole Program Points-to Analysis\n");

  SVFModule *svfModule =
      LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(moduleNameVec);
  svfModule->buildSymbolTableInfo();

  /// Build Program Assignment Graph (PAG)
  PAGBuilder builder;
  PAG *pag = builder.build(svfModule);
  // pag->dump("pag");

  /// Create Andersen's pointer analysis
  Andersen *ander = AndersenWaveDiff::createAndersenWaveDiff(pag);

  /// Query aliases
  /// aliasQuery(ander,value1,value2);

  /// Print points-to information
  /// printPts(ander, value1);

  /// Call Graph
  PTACallGraph *callgraph = ander->getPTACallGraph();
  // callgraph->dump("callgraph");

  /// ICFG
  ICFG *icfg = pag->getICFG();
  // icfg->dump("icfg");

  /// Value-Flow Graph (VFG)
  VFG *vfg = new VFG(callgraph);
  // vfg->dump("vfg");

  /// Sparse value-flow graph (SVFG)
  SVFGBuilder svfBuilder;
  SVFG *svfg = svfBuilder.buildFullSVFGWithoutOPT(ander);
  // svfg->dump("svfg");

  /// Collect uses of an LLVM Value
  /// traverseOnVFG(svfg, value);

  /// Collect all successor nodes on ICFG
  /// traverseOnICFG(icfg, value);

  LeakChecker *saber = new LeakChecker(); // if no checker is specified, we use
                                          // leak checker as the default one.
  saber->runOnModule(svfModule);

  PathCondAllocator *pca = new PathCondAllocator();
  pca->allocate(svfModule);
  pca->printPathCond();

  // Detector *detector = new Detector();
  // detector->runOnModule(svfModule);

  traverseCallGraph(callgraph);
  LLVMModuleSet::getLLVMModuleSet()->dumpModulesToFile(".svf.bc");

  return 0;
}
