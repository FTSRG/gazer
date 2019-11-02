//==- ModuleToAutomata.h ----------------------------------------*- C++ -*--==//
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
#ifndef GAZER_MODULETOAUTOMATA_H
#define GAZER_MODULETOAUTOMATA_H

#include "gazer/Automaton/Cfa.h"
#include "gazer/LLVM/LLVMFrontendSettings.h"
#include "gazer/LLVM/LLVMTraceBuilder.h"

#include <llvm/Pass.h>

namespace llvm
{
    class LoopInfo;
    class Value;
}

namespace gazer::llvm2cfa
{
    class GenerationContext;
} // end namespace gazer::llvm2cfa

namespace gazer
{

class MemoryModel;

/// Provides a mapping between CFA locations/variables and LLVM values.
class CfaToLLVMTrace
{
    friend class llvm2cfa::GenerationContext;
public:
    enum LocationKind { Location_Unknown, Location_Entry, Location_Exit };

    struct BlockToLocationInfo
    {
        llvm::BasicBlock* block;
        LocationKind kind;
    };

    struct ValueMappingInfo
    {
        llvm::DenseMap<const llvm::Value*, ExprPtr> values;
    };

    BlockToLocationInfo getBlockFromLocation(Location* loc) {
        return mLocationsToBlocks.lookup(loc);
    }

    ExprPtr getExpressionForValue(const Cfa* parent, const llvm::Value* value);
    Variable* getVariableForValue(const Cfa* parent, const llvm::Value* value);

private:
    llvm::DenseMap<const Location*, BlockToLocationInfo> mLocationsToBlocks;
    llvm::DenseMap<const Cfa*, ValueMappingInfo> mValueMaps;
};

class ModuleToAutomataPass : public llvm::ModulePass
{
public:
    static char ID;

    ModuleToAutomataPass(GazerContext& context, LLVMFrontendSettings settings)
        : ModulePass(ID), mContext(context), mSettings(settings)
    {}

    void getAnalysisUsage(llvm::AnalysisUsage& au) const override;

    bool runOnModule(llvm::Module& module) override;

    llvm::StringRef getPassName() const override {
        return "Module to automata transformation";
    }

    AutomataSystem& getSystem() { return *mSystem; }
    llvm::DenseMap<llvm::Value*, Variable*>& getVariableMap() {
        return mVariables;
    }
    CfaToLLVMTrace& getTraceInfo() {
        return mTraceInfo;
    }

private:
    std::unique_ptr<AutomataSystem> mSystem;
    llvm::DenseMap<llvm::Value*, Variable*> mVariables;
    CfaToLLVMTrace mTraceInfo;
    GazerContext& mContext;
    LLVMFrontendSettings mSettings;
};

std::unique_ptr<AutomataSystem> translateModuleToAutomata(
    llvm::Module& module,
    LLVMFrontendSettings settings,
    llvm::DenseMap<llvm::Function*, llvm::LoopInfo*>& loopInfos,
    GazerContext& context,
    MemoryModel& memoryModel,
    llvm::DenseMap<llvm::Value*, Variable*>& variables,
    CfaToLLVMTrace& blockEntries
);

llvm::Pass* createCfaPrinterPass();

llvm::Pass* createCfaViewerPass();

}

#endif //GAZER_MODULETOAUTOMATA_H
