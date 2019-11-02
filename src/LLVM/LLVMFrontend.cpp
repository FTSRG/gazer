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
#include "gazer/LLVM/LLVMFrontend.h"
#include "gazer/LLVM/Instrumentation/DefaultChecks.h"
#include "gazer/LLVM/InstrumentationPasses.h"
#include "gazer/LLVM/Transform/Passes.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/LLVM/Transform/UndefToNondet.h"
#include "gazer/Trace/TraceWriter.h"
#include "gazer/LLVM/Trace/TestHarnessGenerator.h"

#include <llvm/Analysis/BasicAliasAnalysis.h>
#include <llvm/Analysis/GlobalsModRef.h>
#include <llvm/Analysis/MemorySSA.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/AlwaysInliner.h>
#include <llvm/Support/CommandLine.h>

#include <llvm/Analysis/CFGPrinter.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Bitcode/BitcodeWriter.h>

using namespace gazer;
using namespace llvm;

namespace gazer
{
    extern cl::OptionCategory LLVMFrontendCategory;
} // end namespace gazer

namespace
{
    cl::opt<bool> ShowFinalCFG(
        "show-final-cfg", cl::desc("Display the final CFG"), cl::cat(LLVMFrontendCategory));

    class RunVerificationBackendPass : public llvm::ModulePass
    {
    public:
        static char ID;

        RunVerificationBackendPass(
            const CheckRegistry& checks,
            VerificationAlgorithm& algorithm,
            const LLVMFrontendSettings& settings
        ) : ModulePass(ID), mChecks(checks), mAlgorithm(algorithm), mSettings(settings)
        {}

        void getAnalysisUsage(llvm::AnalysisUsage& au) const override
        {
            au.addRequired<ModuleToAutomataPass>();
            au.setPreservesCFG();
        }

        bool runOnModule(llvm::Module& module) override;

    private:
        const CheckRegistry& mChecks;
        VerificationAlgorithm& mAlgorithm;
        const LLVMFrontendSettings& mSettings;
        std::unique_ptr<VerificationResult> mResult;
    };

} // end anonymous namespace

char RunVerificationBackendPass::ID;

LLVMFrontend::LLVMFrontend(
    std::unique_ptr<llvm::Module> module,
    GazerContext& context,
    LLVMFrontendSettings settings
)
    : mContext(context),
    mModule(std::move(module)),
    mChecks(mModule->getContext()),
    mSettings(settings)
{}

void LLVMFrontend::registerVerificationPipeline()
{
    // 1) Do basic preprocessing: get rid of alloca's and turn undef's
    //  into nondet function calls.
    mPassManager.add(llvm::createPromoteMemoryToRegisterPass());
    mPassManager.add(new gazer::UndefToNondetCallPass());

    // 2) Execute early optimization passes.
    registerEarlyOptimizations();

    // 3) Perform check instrumentation.
    registerEnabledChecks();

    // 4) Inline functions and global variables if requested.
    mPassManager.add(gazer::createMarkFunctionEntriesPass());
    registerInliningIfEnabled();

    // 5) Run assertion lifting.
    if (mSettings.liftAsserts) {
        mPassManager.add(new llvm::CallGraphWrapperPass());
        mPassManager.add(gazer::createLiftErrorCallsPass());

        // Assertion lifting creates a lot of dead code. Run a lightweight DCE pass to clean up.
        // FIXME: Maybe this should be a DeadInstEliminationPass?
        mPassManager.add(llvm::createDeadCodeEliminationPass());
    }

    // 6) Execute late optimization passes.
    registerLateOptimizations();

    // 7) Create memory models.
    mPassManager.add(new llvm::BasicAAWrapperPass());
    mPassManager.add(new llvm::GlobalsAAWrapperPass());
    mPassManager.add(new llvm::MemorySSAWrapperPass());

    // 8) Do an instruction namer pass.
    mPassManager.add(llvm::createInstructionNamerPass());

    // Display the final LLVM CFG now.
    if (ShowFinalCFG) {
        mPassManager.add(llvm::createCFGPrinterLegacyPassPass());
    }

    // 9) Perform module-to-automata translation.
    mPassManager.add(new gazer::ModuleToAutomataPass(mContext, mSettings));

    // 10) Execute the verifier backend if there is one.
    if (mBackendAlgorithm != nullptr) {
        mPassManager.add(new RunVerificationBackendPass(mChecks, *mBackendAlgorithm, mSettings));
    }
}

bool RunVerificationBackendPass::runOnModule(llvm::Module& module)
{
    auto& moduleToCfa = getAnalysis<ModuleToAutomataPass>();

    AutomataSystem& system = moduleToCfa.getSystem();
    CfaToLLVMTrace cfaToLlvmTrace = moduleToCfa.getTraceInfo();
    LLVMTraceBuilder traceBuilder{system.getContext(), cfaToLlvmTrace};

    mResult = mAlgorithm.check(system, traceBuilder);

    if (auto fail = llvm::dyn_cast<FailResult>(mResult.get())) {
        unsigned ec = fail->getErrorID();
        std::string msg = mChecks.messageForCode(ec);

        llvm::outs() << "Verification FAILED.\n";
        llvm::outs() << "  " << msg << "\n";

        if (mSettings.trace) {
            auto writer = trace::CreateTextWriter(llvm::outs(), true);
            llvm::outs() << "Error trace:\n";
            llvm::outs() << "------------\n";
            if (fail->hasTrace()) {
                writer->write(fail->getTrace());
            } else {
                llvm::outs() << "Error trace is unavailable.\n";
            }
        }

        if (!mSettings.testHarnessFile.empty()) {
            llvm::outs() << "Generating test harness.\n";
            auto test = GenerateTestHarnessModuleFromTrace(
                fail->getTrace(), 
                module.getContext(),
                module
            );

            llvm::StringRef filename(mSettings.testHarnessFile);
            std::error_code osError;
            llvm::raw_fd_ostream testOS(filename, osError, llvm::sys::fs::OpenFlags::OF_None);

            if (filename.endswith("ll")) {
                testOS << *test;
            } else {
                llvm::WriteBitcodeToFile(*test, testOS);
            }
        }
    } else if (mResult->isSuccess()) {
        llvm::outs() << "Verification SUCCESSFUL.\n";
    } else if (auto internalError = llvm::dyn_cast<InternalErrorResult>(mResult.get())) {
        llvm::outs() << "Verification INTERNAL ERROR.\n";
        llvm::outs() << "  " << internalError->getMessage() << "\n";
    } else if (mResult->getStatus() == VerificationResult::Timeout) {
        llvm::outs() << "Verification TIMEOUT\n";
    } else {
        llvm::outs() << "Verification UNKNOWN\n";
    }

    return false;
}

void LLVMFrontend::registerEnabledChecks()
{
    mChecks.add(checks::CreateAssertionFailCheck());
    mChecks.add(checks::CreateDivisionByZeroCheck());
    mChecks.registerPasses(mPassManager);
}

void LLVMFrontend::registerInliningIfEnabled()
{
    if (mSettings.inlineFunctions) {
        // Mark all functions but the main as 'always inline'
        for (auto &func : mModule->functions()) {
            // Ignore the main function and declaration-only functions
            if (func.getName() != "main" && !func.isDeclaration()) {
                func.addAttribute(llvm::AttributeList::FunctionIndex, llvm::Attribute::AlwaysInline);
                func.setLinkage(GlobalValue::InternalLinkage);
            }
        }

        // Mark globals as internal
        for (auto &gv : mModule->globals()) {
            gv.setLinkage(GlobalValue::InternalLinkage);
        }

        mPassManager.add(llvm::createAlwaysInlinerLegacyPass());

        if (mSettings.inlineGlobals) {
            mPassManager.add(createInlineGlobalVariablesPass());
        }

        // Transform the generated alloca instructions into registers
        mPassManager.add(llvm::createPromoteMemoryToRegisterPass());
    }
}

void LLVMFrontend::registerPass(llvm::Pass* pass)
{
    mPassManager.add(pass);
}

void LLVMFrontend::run()
{
    mPassManager.run(*mModule);
}

void LLVMFrontend::registerEarlyOptimizations()
{
    if (mSettings.optimize) {
        mPassManager.add(llvm::createCFGSimplificationPass());
        mPassManager.add(llvm::createSROAPass());
    
        mPassManager.add(llvm::createIPSCCPPass());
        mPassManager.add(llvm::createGlobalOptimizerPass());
        mPassManager.add(llvm::createDeadArgEliminationPass());
    }
}

void LLVMFrontend::registerLateOptimizations()
{
    if (mSettings.optimize) {
        mPassManager.add(llvm::createFloat2IntPass());

        mPassManager.add(llvm::createIndVarSimplifyPass());
        mPassManager.add(llvm::createLICMPass());
    }

    // Currently loop simplify must be applied for ModuleToAutomata
    // to work properly as it relies on loop preheaders being available.
    mPassManager.add(llvm::createCFGSimplificationPass());
    mPassManager.add(llvm::createLoopSimplifyPass());
}

auto LLVMFrontend::FromInputFile(
    llvm::StringRef input,
    GazerContext& context,
    llvm::LLVMContext& llvmContext,
    LLVMFrontendSettings settings
) -> std::unique_ptr<LLVMFrontend>
{
    llvm::SMDiagnostic err;

    std::unique_ptr<llvm::Module> module = nullptr;

    if (input.endswith(".bc")) {
        module = llvm::parseIRFile(input, err, llvmContext);
    } else if (input.endswith(".ll")) {
        module = llvm::parseAssemblyFile(input, err, llvmContext);
    } else {
        err = SMDiagnostic(
            input,
            SourceMgr::DK_Error,
            "Input file must be in LLVM bitcode (.bc) or LLVM assembly (.ll) format."
        );
    }

    if (module == nullptr) {
        err.print(nullptr, llvm::errs());
        return nullptr;
    }

    return std::make_unique<LLVMFrontend>(std::move(module), context, settings);
}