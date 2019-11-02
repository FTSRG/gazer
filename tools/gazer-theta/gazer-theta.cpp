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
#include "lib/ThetaVerifier.h"
#include "lib/ThetaCfaGenerator.h"

#include "gazer/LLVM/ClangFrontend.h"
#include "gazer/LLVM/LLVMFrontend.h"
#include "gazer/Core/GazerContext.h"

#include <llvm/IR/Module.h>
#include <boost/dll/runtime_symbol_info.hpp>

#ifndef NDEBUG
#include <llvm/Support/Debug.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#endif

using namespace gazer;
using namespace llvm;
using namespace llvm;

namespace gazer
{
    extern cl::OptionCategory LLVMFrontendCategory;
    extern cl::OptionCategory IrToCfaCategory;
    extern cl::OptionCategory TraceCategory;
    extern cl::OptionCategory ClangFrontendCategory;
}

namespace
{
    cl::list<std::string> InputFilenames(cl::Positional, cl::OneOrMore, cl::desc("<input files>"));

    // Theta environment settings
    cl::OptionCategory ThetaEnvironmentCategory("Theta environment settings");

    cl::opt<std::string> ModelPath("o",
        cl::desc("Model output path (will use a unique location if not set)"),
        cl::cat(ThetaEnvironmentCategory),
        cl::init("")
    );
    cl::opt<bool> ModelOnly("model-only",
        cl::desc("Do not run the verification engine, just write the theta CFA"),
        cl::cat(ThetaEnvironmentCategory)
    );

    cl::opt<std::string> ThetaPath("theta-path",
        cl::desc("Full path to the theta-cfa jar file. Defaults to '<path_to_this_binary>/theta/theta-cfa-cli.jar'"),
        cl::cat(ThetaEnvironmentCategory),
        cl::init("")
    );
    cl::opt<std::string> LibPath("lib-path",
        cl::desc("Full path to the directory containing the Z3 libraries (libz3.so, libz3java.so) required by theta."
                 " Defaults to '<path_to_this_binary>/theta/lib'"),
        cl::cat(ThetaEnvironmentCategory),
        cl::init("")
    );

    // Algorithm options
    cl::OptionCategory ThetaAlgorithmCategory("Theta algorithm settings");

    cl::opt<std::string> Domain("domain", cl::desc("Abstract domain"), cl::init("PRED_CART"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> Refinement("refinement", cl::desc("Refinement strategy"), cl::init("SEQ_ITP"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> Search("search", cl::desc("Search strategy"), cl::init("BFS"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> PrecGranularity("precgranularity", cl::desc("Precision granularity"), cl::init("GLOBAL"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> PredSplit("predsplit", cl::desc("Predicate splitting (for predicate abstraction)"), cl::init("WHOLE"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> Encoding("encoding", cl::desc("Block encoding"), cl::init("LBE"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<int> MaxEnum("maxenum", cl::desc("Maximal number of explicitly enumerated successors"), cl::init(0), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> InitPrec("initPrec", cl::desc("Initial precision of abstraction"), cl::init("EMPTY"), cl::cat(ThetaAlgorithmCategory));
} // end anonymous namespace

static theta::ThetaSettings initSettingsFromCommandLine();

int main(int argc, char* argv[])
{
    cl::HideUnrelatedOptions({
        &LLVMFrontendCategory, &ThetaAlgorithmCategory, &ThetaEnvironmentCategory, &IrToCfaCategory,
        &TraceCategory, &ClangFrontendCategory
    });
    cl::SetVersionPrinter([](llvm::raw_ostream& os) {
        os << "gazer - a formal verification frontend\n";
        os << "   version 0.1\n";
        os << "   LLVM version 9.0\n";
    });
    cl::ParseCommandLineOptions(argc, argv);

    #ifndef NDEBUG
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::PrettyStackTraceProgram(argc, argv);
    llvm::EnableDebugBuffering = true;
    #endif

    // Set up the basics
    GazerContext context;
    llvm::LLVMContext llvmContext;

    // Set up settings
    theta::ThetaSettings backendSettings = initSettingsFromCommandLine();
    auto settings = LLVMFrontendSettings::initFromCommandLine();

    // Force -math-int, -inline and -inline-globals
    settings.ints = IntRepresentation::Integers;
    settings.inlineFunctions = true;
    settings.inlineGlobals = true;

    if (backendSettings.thetaCfaPath.empty() || backendSettings.thetaLibPath.empty()) {
        // Find the current program location
        boost::dll::fs::error_code ec;
        auto pathToBinary = boost::dll::program_location(ec);

        if (ec) {
            llvm::errs() << "ERROR: Could not find the path to this process: " + ec.message();
            return 1;
        }

        if (backendSettings.thetaCfaPath.empty()) {
            backendSettings.thetaCfaPath = pathToBinary.string() + "/theta/theta-cfa-cli.jar";
        }

        if (backendSettings.thetaLibPath.empty()) {
            backendSettings.thetaLibPath = pathToBinary.string() + "/theta/lib";
        }
    }

    // Run the clang frontend
    auto module = ClangCompileAndLink(InputFilenames, llvmContext);
    if (module == nullptr) {
        return 1;
    }

    auto frontend = std::make_unique<LLVMFrontend>(std::move(module), context, settings);

    // TODO: This should be more flexible.
    if (frontend->getModule().getFunction("main") == nullptr) {
        llvm::errs() << "ERROR: No 'main' function found.\n";
        return 1;
    }

    if (!ModelOnly) {
        frontend->setBackendAlgorithm(new theta::ThetaVerifier(backendSettings));
        frontend->registerVerificationPipeline();
        frontend->run();
    } else {
        if (ModelPath.empty()) {
            llvm::errs() << "ERROR: -model-only must be supplied together with -o <path>!\n";
            return 1;
        }

        std::error_code errorCode;
        llvm::raw_fd_ostream rfo(ModelPath, errorCode);

        if (errorCode) {
            llvm::errs() << "ERROR: " << errorCode.message() << "\n";
            return 1;
        }

        // Do not run theta, just generate the model.
        frontend->registerVerificationPipeline();
        frontend->registerPass(theta::createThetaCfaWriterPass(rfo));
        frontend->run();
    }

    llvm::llvm_shutdown();

    return 0;
}

theta::ThetaSettings initSettingsFromCommandLine()
{
    theta::ThetaSettings settings;

    settings.timeout = 0; // TODO
    settings.modelPath = ModelPath;
    settings.domain = Domain;
    settings.refinement = Refinement;
    settings.search = Search;
    settings.precGranularity = PrecGranularity;
    settings.predSplit = PredSplit;
    settings.encoding = Encoding;
    settings.maxEnum = std::to_string(MaxEnum);
    settings.initPrec = InitPrec;
    settings.thetaCfaPath = ThetaPath;
    settings.thetaLibPath = LibPath;

    return settings;
}
