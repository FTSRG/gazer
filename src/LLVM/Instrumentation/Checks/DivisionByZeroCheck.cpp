//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2020 Contributors to the Gazer project
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
#include "gazer/LLVM/Instrumentation/DefaultChecks.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace gazer;
using namespace llvm;

namespace
{

/// Checks for division by zero errors.
class DivisionByZeroCheck final : public Check
{
public:
    static char ID;

    DivisionByZeroCheck()
        : Check(ID)
    {}

    bool mark(llvm::Function& function) override;

    llvm::StringRef getErrorDescription() const override
    {
        return "Divison by zero";
    }
};

} // namespace

char DivisionByZeroCheck::ID;

static bool isDivisionInst(unsigned opcode)
{
    return opcode == Instruction::SDiv || opcode == Instruction::UDiv;
}

bool DivisionByZeroCheck::mark(llvm::Function& function)
{
    auto& context = function.getContext();

    std::vector<llvm::Instruction*> divs;
    for (llvm::Instruction& inst : instructions(function)) {
        if (isDivisionInst(inst.getOpcode())) {
            divs.push_back(&inst);
        }
    }

    if (divs.empty()) {
        return false;
    }

    IRBuilder<> builder(context);
    for (llvm::Instruction* inst : divs) {
        BasicBlock* errorBB = this->createErrorBlock(
            function,
            "error.divzero",
            inst
        );

        BasicBlock* bb = inst->getParent();
        llvm::Value* rhs = inst->getOperand(1);

        builder.SetInsertPoint(inst);
        auto icmp = builder.CreateICmpNE(
            rhs, builder.getInt(llvm::APInt(
                rhs->getType()->getIntegerBitWidth(), 0
            ))
        );

        BasicBlock* newBB = bb->splitBasicBlock(inst);
        builder.ClearInsertionPoint();
        llvm::ReplaceInstWithInst(
            bb->getTerminator(),
            builder.CreateCondBr(icmp, newBB, errorBB)
        );
    }

    return true;
}

std::unique_ptr<Check> gazer::checks::createDivisionByZeroCheck(ClangOptions& options)
{
    return std::make_unique<DivisionByZeroCheck>();
}
