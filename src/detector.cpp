#include "detector.h"
#include "SVF-FE/LLVMUtil.h"
#include "Util/Options.h"

using namespace SVF;
using namespace SVFUtil;

/*!
 * Initialize sources
 */
void Detector::initSrcs() {

  PAG *pag = getPAG();
  ICFG *icfg = pag->getICFG();
  for (PAG::CSToRetMap::iterator it = pag->getCallSiteRets().begin(),
                                 eit = pag->getCallSiteRets().end();
       it != eit; ++it) {
    const RetBlockNode *cs = it->first;
    /// if this callsite return reside in a dead function then we do not care
    /// about its leaks for example instruction p = malloc is in a dead
    /// function, then program won't allocate this memory

    // NOTE: get callSite function name like main
    // const Value *test = cs->getCallSite();
    // if (const Instruction *inst = SVFUtil::dyn_cast<Instruction>(test)) {
    //   SVFUtil::outs() << inst->getParent()->getParent()->getName().str() << "\n";
    // }

    if (isPtrInDeadFunction(cs->getCallSite()))
      continue;

    PTACallGraph::FunctionSet callees;
    getCallgraph()->getCallees(cs->getCallBlockNode(), callees);
    for (PTACallGraph::FunctionSet::const_iterator cit = callees.begin(),
                                                   ecit = callees.end();
         cit != ecit; cit++) {
      const SVFFunction *fun = *cit;
      SVFUtil::outs() << fun->getName().str() << "\n";
      if (isSourceLikeFun(fun)) {
        CSWorkList worklist;
        SVFGNodeBS visited;
        worklist.push(it->first->getCallBlockNode());
        while (!worklist.empty()) {
          const CallBlockNode *cs = worklist.pop();
          const RetBlockNode *retBlockNode =
              icfg->getRetBlockNode(cs->getCallSite());
          const PAGNode *pagNode = pag->getCallSiteRet(retBlockNode);
          const SVFGNode *node = getSVFG()->getDefSVFGNode(pagNode);
          if (visited.test(node->getId()) == 0)
            visited.set(node->getId());
          else
            continue;

          CallSiteSet csSet;
          // if this node is in an allocation wrapper, find all its call nodes
          if (isInAWrapper(node, csSet)) {
            for (CallSiteSet::iterator it = csSet.begin(), eit = csSet.end();
                 it != eit; ++it) {
              worklist.push(*it);
            }
          }
          // otherwise, this is the source we are interested
          else {
            // exclude sources in dead functions
            if (isPtrInDeadFunction(cs->getCallSite()) == false) {
              addToSources(node);
              addSrcToCSID(node, cs);
            }
          }
        }
      }
    }
  }
}

/*!
 * Initialize sinks
 */
void Detector::initSnks() {

  PAG *pag = getPAG();

  for (PAG::CSToArgsListMap::iterator it = pag->getCallSiteArgsMap().begin(),
                                      eit = pag->getCallSiteArgsMap().end();
       it != eit; ++it) {

    PTACallGraph::FunctionSet callees;
    getCallgraph()->getCallees(it->first, callees);
    for (PTACallGraph::FunctionSet::const_iterator cit = callees.begin(),
                                                   ecit = callees.end();
         cit != ecit; cit++) {
      const SVFFunction *fun = *cit;
      if (isSinkLikeFun(fun)) {
        PAG::PAGNodeList &arglist = it->second;
        assert(!arglist.empty() && "no actual parameter at deallocation site?");
        /// we only choose pointer parameters among all the actual parameters
        for (PAG::PAGNodeList::const_iterator ait = arglist.begin(),
                                              aeit = arglist.end();
             ait != aeit; ++ait) {
          const PAGNode *pagNode = *ait;
          if (pagNode->isPointer()) {
            const SVFGNode *snk =
                getSVFG()->getActualParmVFGNode(pagNode, it->first);
            addToSinks(snk);
          }
        }
      }
    }
  }
}

void Detector::reportBug(ProgSlice *slice) {

  // if (isAllPathReachable() == false && isSomePathReachable() == true)
  // {
  //     reportPartialLeak(slice->getSource());
  //     SVFUtil::errs() << "\t\t conditional free path: \n" <<
  //     slice->evalFinalCond() << "\n"; slice->annotatePaths();
  // }
  return;
}
