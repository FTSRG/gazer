//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
#ifndef GAZER_LLVM_LLVMTRACEBUILDER_H
#define GAZER_LLVM_LLVMTRACEBUILDER_H

#include "gazer/Trace/Trace.h"
#include "gazer/Automaton/Cfa.h"
#include "gazer/Verifier/VerificationAlgorithm.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/InstVisitor.h>

namespace gazer
{

class CfaToLLVMTrace;

class LLVMTraceBuilder : public CfaTraceBuilder
{
public:
    LLVMTraceBuilder(GazerContext& context, CfaToLLVMTrace& cfaToLlvmTrace)
        : mContext(context), mCfaToLlvmTrace(cfaToLlvmTrace)
    {}

    std::unique_ptr<Trace> build(
        std::vector<Location*>& states,
        std::vector<std::vector<VariableAssignment>>& actions
    ) override;

private:
    void handleGazerWriteIntrinsic(
        const llvm::CallInst* call,
        const Location* loc,
        Valuation& currentVals,
        std::vector<std::unique_ptr<TraceEvent>>& events);

    Type* preferredTypeFromDIType(llvm::DIType* diTy);
    ExprRef<AtomicExpr> getLiteralFromLLVMConst(const llvm::ConstantData* value, Type* preferredType = nullptr);
    ExprRef<AtomicExpr> getLiteralFromValue(
        Cfa* cfa, const llvm::Value* value, Valuation& model, Type* preferredType = nullptr
    );

    static TraceVariable traceVarFromDIVar(const llvm::DIVariable* diVar);

private:
    GazerContext& mContext;
    CfaToLLVMTrace& mCfaToLlvmTrace;
};

} // end namespace gazer

#endif
