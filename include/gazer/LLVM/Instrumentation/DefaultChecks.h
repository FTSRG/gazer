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
#include "gazer/LLVM/ClangFrontend.h"
#include "gazer/LLVM/Instrumentation/Check.h"

namespace gazer::checks
{
    /// Check for assertion violations within the program.
    std::unique_ptr<Check> createAssertionFailCheck(ClangOptions& options);

    /// This check fails if a division instruction is reachable
    /// with its second operand's value being 0.
    std::unique_ptr<Check> createDivisionByZeroCheck(ClangOptions& options);

    /// This check fails if a signed integer operation results
    /// in an over- or underflow.
    std::unique_ptr<Check> createSignedIntegerOverflowCheck(ClangOptions& options);

} // end namespace gazer::checks