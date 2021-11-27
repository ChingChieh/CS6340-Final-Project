#include "Graphs/SVFG.h"
#include "SABER/LeakChecker.h"
#include "SABER/SrcSnkDDA.h"
#include "SVF-FE/LLVMUtil.h"
#include "SVF-FE/PAGBuilder.h"
#include "Util/PathCondAllocator.h"
#include "WPA/Andersen.h"
#include "detector.h"

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
    SVFUtil::outs() << "second loop \n";
    const CallBlockNode *cbn = i->first;
    const PTACallGraph::CallGraphEdgeSet ces = i->second;
    PTACallGraph::FunctionSet fset;
    callgraph->getCallees(cbn, fset);
    for (auto j = fset.begin(); j != fset.end(); j++) {
      const SVFFunction *fun = *j;
      SVFUtil::outs() << fun->getName().str() << "\n";
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
