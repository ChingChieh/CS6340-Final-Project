#ifndef DETECTOR_H_
#define DETECTOR_H_

#include "SABER/SaberCheckerAPI.h"
#include "SABER/SrcSnkDDA.h"

namespace SVF {

/*!
 * Static Memory Leak Detector
 */
class Detector : public SrcSnkDDA {

public:
  typedef Map<const SVFGNode *, const CallBlockNode *> SVFGNodeToCSIDMap;
  typedef FIFOWorkList<const CallBlockNode *> CSWorkList;
  typedef ProgSlice::VFWorkList WorkList;
  typedef NodeBS SVFGNodeBS;
  enum LEAK_TYPE { NEVER_FREE_LEAK, CONTEXT_LEAK, PATH_LEAK, GLOBAL_LEAK };

  /// Constructor
  Detector() {}
  /// Destructor
  virtual ~Detector() {}

  /// We start from here
  virtual bool runOnModule(SVFModule *module) {
    /// start analysis
    analyze(module);
    return false;
  }

  /// Initialize sources and sinks
  //@{
  /// Initialize sources and sinks
  virtual void initSrcs() override;
  virtual void initSnks() override;
  /// Whether the function is a heap allocator/reallocator (allocate memory)
  virtual inline bool isSourceLikeFun(const SVFFunction *fun) override{
    if (fun->getName().str() == "main"){
      SVFUtil::outs() << "find main\n";
      return true;
    }
    return false;
  }
  /// Whether the function is a heap deallocator (free/release memory)
  virtual inline bool isSinkLikeFun(const SVFFunction *fun) override {
    return false;
  }
  //@}

protected:
  virtual void reportBug(ProgSlice *slice) override;

  /// Record a source to its callsite
  //@{
  inline void addSrcToCSID(const SVFGNode *src, const CallBlockNode *cs) {
    srcToCSIDMap[src] = cs;
  }
  inline const CallBlockNode *getSrcCSID(const SVFGNode *src) {
    SVFGNodeToCSIDMap::iterator it = srcToCSIDMap.find(src);
    assert(it != srcToCSIDMap.end() && "source node not at a callsite??");
    return it->second;
  }
  //@}
private:
  SVFGNodeToCSIDMap srcToCSIDMap;
};

} // End namespace SVF

#endif
