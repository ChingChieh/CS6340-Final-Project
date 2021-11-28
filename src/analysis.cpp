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
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <vector>

using namespace SVF;
using namespace llvm;
using namespace std;

static llvm::cl::opt<std::string>
    InputFilename(cl::Positional, llvm::cl::desc("<input bitcode>"),
                  llvm::cl::init("-"));

static llvm::cl::opt<std::string>
    OutputFilename("output", llvm::cl::desc("Output File Name"));

static llvm::cl::alias OutputFilename2("o", llvm::cl::aliasopt(OutputFilename));

set<string> target_functions{"g_print", "g_string_printf"};

class Record {
public:
  Record(string callback_name = "", string event_type = "", string caller = "",
         string callee = "", string widget_name = "")
      : callback_name(callback_name), event_type(event_type), caller(caller),
        callee(callee), widget_name(widget_name) {}
  string callback_name;
  string event_type;
  string caller;
  string callee;
  string widget_name;
  u32_t line, col;
  string filename;
  bool isValid() {
    return !callback_name.empty() && !event_type.empty() && !caller.empty() &&
           !callee.empty();
  }
  friend ostream &operator<<(ostream &os, const Record &r);
  friend SVF::raw_ostream &operator<<(SVF::raw_ostream &os, const Record &r);
};

ostream &operator<<(ostream &os, const Record &r) {
  os << fmt::format(
      "Record callback: {}, event: {}, caller: {}, callee: {}, widget: {}",
      r.callback_name, r.event_type, r.caller, r.callee, r.widget_name);
  if (!r.filename.empty()) {
    os << fmt::format(" location: {}:{}:{}", r.filename, r.line, r.col);
  }
  return os;
}

SVF::raw_ostream &operator<<(SVF::raw_ostream &os, const Record &r) {
  os << fmt::format(
      "Record callback: {}, event: {}, caller: {}, callee: {}, widget: {}",
      r.callback_name, r.event_type, r.caller, r.callee, r.widget_name);
  if (!r.filename.empty()) {
    os << fmt::format(" location: {}:{}:{}", r.filename, r.line, r.col);
  }
  return os;
}

void analyze(vector<Record> &records, vector<string> &interesting_functions,
             map<string, vector<string>> &interesting_map) {
  fstream fs;
  bool first = true;
  if (!OutputFilename.empty()) {
    fs.open(OutputFilename, fstream::out);
    if (fs.fail()) {
      SVFUtil::errs() << "Cannot open output file\n";
      exit(1);
    }
  }
  SVFUtil::outs() << "start analyzing \n";
  SVFUtil::outs() << "list records \n";
  for (Record &r : records) {
    SVFUtil::outs() << r << "\n";
  }
  for (auto &it : interesting_map) {
    for (auto &caller_name : it.second) {
      // SVFUtil::outs() << "found interesting function: " << it.first << "\n";
      SVFUtil::outs() << fmt::format("Found interesting function: {} in {} \n",
                                     it.first, caller_name);
      SVFUtil::outs() << fmt::format(
          "Try to found corresponding g_singal_connect \n");
      // SVFUtil::outs() << "caller: " << caller_name << "\n";

      // NOTE: iterate records to find matching callback
      for (Record &r : records) {
        if (r.callback_name == caller_name) {
          stringstream ss;
          if (!first) {
            ss << "===================================\n";
          }
          ss << "Found a match!\n";
          ss << fmt::format("interesting function: {}\n", it.first);
          ss << fmt::format("in callback function: {}\n", caller_name);
          ss << fmt::format("event type {}\n", r.event_type);
          ss << fmt::format("widget name: {}\n", r.widget_name);
          ss << fmt::format("g_signal_connect source location: {}:{}:{}\n",
                            r.filename, r.line, r.col);
          // ss << "[MATCH] " << r << "\n";
          SVFUtil::outs() << ss.str();
          if (fs.is_open()) {
            fs << ss.str();
          }
          first = false;
        }
      }
    }
    SVFUtil::outs() << "================\n";
  }
  if (fs.is_open()) {
    fs.close();
  }
}

void traverseCallGraph(PTACallGraph *callgraph) {
  // NOTE: include/Graphs/PTACallGraph.h:218:0
  vector<Record> records;
  vector<string> interesting_functions;
  map<string, vector<string>> interesting_map;

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
    // SVFUtil::outs() << "second loop ==============\n";
    // NOTE: include/Graphs/ICFGNode.h:364:0
    const CallBlockNode *cbn = i->first;
    const PTACallGraph::CallGraphEdgeSet ces = i->second;
    const SVFFunction *caller = cbn->getCaller();
    const Instruction *callsite = cbn->getCallSite();

    const DebugLoc &DL = callsite->getDebugLoc();
    const CallBlockNode::ActualParmVFGNodeVec &params = cbn->getActualParms();
    string caller_name = caller->getName().str();

    PTACallGraph::FunctionSet fset;
    callgraph->getCallees(cbn, fset);
    for (auto j = fset.begin(); j != fset.end(); j++) {
      const SVFFunction *callee = *j;
      u32_t arg_size = callee->arg_size();
      string callee_name = callee->getName().str();
      if (target_functions.find(callee_name) != target_functions.end()) {
        SVFUtil::outs() << "Found interesting " << callee_name << caller_name
                        << "\n";
        interesting_map[callee_name].push_back(caller_name);
        interesting_functions.push_back(caller_name);
        continue;
      } else if (callee_name != "g_signal_connect_data") {
        continue;
      }

      Record r;
      r.caller = caller_name;
      r.callee = callee_name;

      SVFUtil::outs() << "caller: " << caller->getName().str() << "\n";
      SVFUtil::outs() << "callee: " << callee_name << "\n";
      if (DL) {
        DIScope *Scope = cast<DIScope>(DL->getScope());
        string filename = Scope->getFilename().str();
        u32_t line = DL.getLine();
        u32_t col = DL.getCol();
        // SVFUtil::outs() << "DL: " << *DL << "\n";
        SVFUtil::outs() << fmt::format("file: {}, line: {}, col: {}\n",
                                       filename, line, col);
        r.line = line;
        r.col = col;
        r.filename = filename;
      }
      SVFUtil::outs() << "opcode name: " << callsite->getOpcodeName() << "\n";
      // SVFUtil::outs() << "arg_size: " << arg_size << "\n";
      // SVFUtil::outs() << "arg_size2: " << params.size() << "\n";
      // for (u32_t arg_num = 0; arg_num < arg_size; arg_num++) {
      //   const Value *arg = callsite->getOperand(arg_num);
      //   SVFUtil::outs() << "arg_name: " << arg->getName() << "\n";
      // }
      int index = 0;
      for (auto &p : params) {
        const Type *t = p->getType();
        // SVFUtil::outs() << p->getValueName() << "\n";
        p->dump();
        if (p->hasValue()) {
          const Value *v = p->getValue();
          // SVFUtil::outs() << "get value " << *v << "\n";
          // SVFUtil::outs() << "get type " << *t << "\n";
          if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(v)) {
            // SVFUtil::outs() << "is constant expr \n";
            if (const BitCastInst *BI = dyn_cast<BitCastInst>(v)) {
              // SVFUtil::outs() << "is constant expr BITCAST \n";
            }
            const Value *op0 = CE->getOperand(0);
            // SVFUtil::outs() << *op0 << "\n";
            if (const Function *F = dyn_cast<Function>(op0)) {
              // SVFUtil::outs() << "OP0 is FUNCTION \n";
              // NOTE: get the print_hello here !!
              const string callback_name = F->getName().str();
              r.callback_name = callback_name;
              // SVFUtil::outs() << Fname << "\n";
              SVFUtil::outs()
                  << fmt::format("callback name is {}!\n", callback_name);
            }

            // SVFUtil::outs() << *(CE->getOperand(0)) << "\n";
          } else if (const ConstantArray *CA = dyn_cast<ConstantArray>(v)) {
            // SVFUtil::outs() << "is constant array "
            //                 << "\n";
          } else if (const GetElementPtrInst *GEP =
                         dyn_cast<GetElementPtrInst>(v)) {
            // NOTE: find "clicked"
            // SVFUtil::outs() << "is GEP"
            //                 << "\n";
            const Value *gv = GEP->getPointerOperand();
            // SVFUtil::outs() << *gv << "\n";
            if (const GlobalVariable *GV = dyn_cast<GlobalVariable>(gv)) {
              // SVFUtil::outs() << "IS_GV" << *GV << "\n";
              if (const ConstantDataArray *CDA =
                      dyn_cast<ConstantDataArray>(GV->getInitializer())) {
                // SVFUtil::outs() << "IS_CDA"
                //                 << "\n";
                string event_type = CDA->getAsCString().str();
                r.event_type = event_type;
                // SVFUtil::outs() << s << "\n";
                SVFUtil::outs()
                    << fmt::format("event type is {}\n", event_type);
              }
            }
          } else if (const BitCastInst *BI = dyn_cast<BitCastInst>(v)) {
            // SVFUtil::outs() << "IS BITCAST" << *BI << "\n";
            const Value *op0 = (BI->getOperand(0));
            if (const LoadInst *LI = dyn_cast<LoadInst>(op0)) {
              const Value *op0 = (LI->getOperand(0));
              const string name = op0->getName().str();
              SVFUtil::outs() << fmt::format("name is {}\n", name);
              r.widget_name = name;
            }
          }
          index++;
        }
      }
      //
      // SVFUtil::outs() << r << "\n";
      records.push_back(r);
    }
  }
  analyze(records, interesting_functions, interesting_map);
}

int main(int argc, char **argv) {

  int arg_num = 0;
  char **arg_value = new char *[argc];
  std::vector<std::string> moduleNameVec;
  SVFUtil::processArguments(argc, argv, arg_num, arg_value, moduleNameVec);
  cl::ParseCommandLineOptions(arg_num, arg_value, "GUI Program Analysis\n");

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

  traverseCallGraph(callgraph);
  LLVMModuleSet::getLLVMModuleSet()->dumpModulesToFile(".svf.bc");

  return 0;
}
