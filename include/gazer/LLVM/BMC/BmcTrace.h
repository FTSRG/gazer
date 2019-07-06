#ifndef _GAZER_LLVM_ANALYSIS_BMCTRACE_H
#define _GAZER_LLVM_ANALYSIS_BMCTRACE_H

#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/Valuation.h"
#include "gazer/LLVM/Ir2Expr.h"
#include "gazer/LLVM/Analysis/TopologicalSort.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/ADT/DenseMap.h>

#include <vector>
#include <string>
#include <gazer/Trace/Trace.h>

namespace llvm {
    class raw_ostream;
}

namespace gazer
{
/*
class LLVMBmcTraceBuilder : public TraceBuilder
{
public:
    LLVMBmcTraceBuilder(
        GazerContext& context,
        const TopologicalSort& topo,
        const llvm::DenseMap<llvm::BasicBlock*, size_t>& blocks,
        const llvm::DenseMap<llvm::BasicBlock*, ExprPtr>& preds,
        const ValueToVariableMap& valueMap,
        llvm::BasicBlock* errorBlock
    ) :
    mContext(context),
    mTopo(topo), mBlocks(blocks), mPreds(preds),
    mValueMap(valueMap), mErrorBlock(errorBlock)
    {}

    LLVMBmcTraceBuilder(const LLVMBmcTraceBuilder&) = delete;
    LLVMBmcTraceBuilder& operator=(const LLVMBmcTraceBuilder&) = delete;

protected:
    std::vector<std::unique_ptr<TraceEvent>> buildEvents(Valuation& model) override;

private:
    ExprRef<AtomicExpr> getLiteralFromValue(llvm::Value* value, Valuation& model);

private:
    GazerContext& mContext;
    const TopologicalSort& mTopo;
    const llvm::DenseMap<llvm::BasicBlock*, size_t> mBlocks;
    const llvm::DenseMap<llvm::BasicBlock*, ExprPtr>& mPreds;
    const ValueToVariableMap& mValueMap;
    llvm::BasicBlock* mErrorBlock;
}; */

} // end namespace gazer

#endif
