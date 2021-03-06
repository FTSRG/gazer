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
#include "gazer/LLVM/Memory/MemoryObject.h"
#include "gazer/LLVM/Memory/MemoryModel.h"

#include <llvm/AsmParser/Parser.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Dominators.h>

#include <gtest/gtest.h>

using namespace gazer;

namespace
{

class MemoryObjectTest : public ::testing::Test
{
public:
    MemoryObjectTest()
        : module(nullptr)
    {}
protected:
    void setUp(const char* moduleStr)
    {
        module = llvm::parseAssemblyString(moduleStr, error, llvmContext);
        if (module == nullptr) {
            error.print("MemoryObjectTest", llvm::errs());
            FAIL() << "Failed to construct LLVM module!\n";
            return;
        }

        analyses.reset(new RequiredAnalyses(*this));
    }

    struct RequiredAnalyses
    {
        std::unordered_map<
            llvm::Function*,
            std::unique_ptr<llvm::DominatorTree>
        > dtMap;

        RequiredAnalyses(MemoryObjectTest& test)
        {
            for (llvm::Function& function : *test.module) {
                if (!function.isDeclaration()) {
                    dtMap[&function] = std::make_unique<llvm::DominatorTree>(function);
                }
            }
        }
    };

protected:
    llvm::LLVMContext llvmContext;
    llvm::SMDiagnostic error;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<RequiredAnalyses> analyses;
};

TEST_F(MemoryObjectTest, CanFindGlobalVariables)
{
    setUp(R"ASM(
@a = global i32 0, align 4
@b = global i32 1, align 4

declare i32 @__VERIFIER_nondet_int()
declare void @__VERIFIER_error()

define i32 @main() #0 {
bb:
  %tmp = call i32 @__VERIFIER_nondet_int()  ; a = __VERIFIER_nondet_int()
  store i32 %tmp, i32* @a, align 4
  %tmp1 = load i32, i32* @a, align 4        ; b = a + 1
  %tmp2 = add nsw i32 %tmp1, 1
  store i32 %tmp2, i32* @b, align 4
  %tmp3 = load i32, i32* @a, align 4        ; if (a == b) { __VERIFIER_error(); }
  %tmp4 = load i32, i32* @b, align 4
  %tmp5 = icmp ne i32 %tmp3, %tmp4
  br i1 %tmp5, label %bb6, label %bb7

bb6:                                              ; preds = %bb
  br label %bb8

bb7:                                              ; preds = %bb
  call void @__VERIFIER_error()
  unreachable

bb8:                                              ; preds = %bb6
  ret i32 0
}
)ASM");

    GazerContext context;
    LLVMFrontendSettings settings;
    
    // TODO
}

} // end anonymous namespace