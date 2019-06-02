#include "gazer/LLVM/Ir2Expr.h"

#include "gazer/Core/Type.h"
#include "gazer/Analysis/LegacyMemoryModel.h"

#include <llvm/IR/InstIterator.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Debug.h>

using namespace gazer;

using llvm::Instruction;
using llvm::BasicBlock;
using llvm::Function;
using llvm::Value;
using llvm::Argument;
using llvm::GlobalVariable;
using llvm::ConstantInt;
using llvm::isa;
using llvm::dyn_cast;

#define DEBUG_TYPE "inst2expr"

namespace gazer {
    extern llvm::cl::opt<bool> AssumeNoNaN;
    extern llvm::cl::opt<bool> UseMathInt;
}

gazer::Type& InstToExpr::typeFromLLVMType(const llvm::Type* type)
{
    if (type->isIntegerTy()) {
        auto width = type->getIntegerBitWidth();
        if (width == 1) {
            return BoolType::Get(mContext);
        }

        //if (UseMathInt && width <= 64) {
        //    return IntType::Get(mContext, width);
        //}

        return BvType::Get(mContext, width);
    } else if (type->isHalfTy()) {
        return FloatType::Get(mContext, FloatType::Half);
    } else if (type->isFloatTy()) {
        return FloatType::Get(mContext, FloatType::Single);
    } else if (type->isDoubleTy()) {
        return FloatType::Get(mContext, FloatType::Double);
    } else if (type->isFP128Ty()) {
        return FloatType::Get(mContext, FloatType::Quad);
    } else if (type->isPointerTy()) {
        return mMemoryModel.getTypeFromPointerType(llvm::cast<llvm::PointerType>(type));
    }

    assert(false && "Unsupported LLVM type.");
}

gazer::Type& InstToExpr::typeFromLLVMType(const llvm::Value* value)
{
    return typeFromLLVMType(value->getType());
}

static bool isLogicInstruction(unsigned opcode) {
    return opcode == Instruction::And || opcode == Instruction::Or || opcode == Instruction::Xor;
}

static bool isFloatInstruction(unsigned opcode) {
    return opcode == Instruction::FAdd || opcode == Instruction::FSub
    || opcode == Instruction::FMul || opcode == Instruction::FDiv;
}

static bool isNonConstValue(const llvm::Value* value) {
    return isa<Instruction>(value) || isa<Argument>(value) || isa<GlobalVariable>(value);
}

InstToExpr::InstToExpr(
    Function& function,
    GazerContext& context,
    ExprBuilder* builder,
    ValueToVariableMap& variables,
    legacy::MemoryModel& memoryModel,
    llvm::DenseMap<llvm::Value*, ExprPtr>& eliminatedValues
)
    : mFunction(function), mContext(context),
    mExprBuilder(builder), mVariables(variables),
    mMemoryModel(memoryModel),
    mEliminatedValues(eliminatedValues)
   // mStack(stack), mHeap(heap)
{
    // Add arguments as local variables
    for (auto& arg : mFunction.args()) {
        Variable* variable = mContext.createVariable(
            arg.getName(),
            typeFromLLVMType(&arg)
        );
        mVariables[&arg] = variable;
    }

    // Add local values as variables
    for (auto& instr : llvm::instructions(function)) {
        if (instr.getName() != "") {
            Variable* variable = mContext.createVariable(
                instr.getName(),
                typeFromLLVMType(&instr)
            );
            mVariables[&instr] = variable;
        }
    }

    // Initialize other variables the memory model might introduce
    mMemoryModel.initialize(function, mVariables);
}

ExprPtr InstToExpr::transform(llvm::Instruction& inst, size_t succIdx, BasicBlock* pred)
{
    if (inst.isTerminator()) {
        if (inst.getOpcode() == Instruction::Br) {
            return handleBr(*dyn_cast<llvm::BranchInst>(&inst), succIdx);
        } else if (inst.getOpcode() == Instruction::Switch) {
            return handleSwitch(*dyn_cast<llvm::SwitchInst>(&inst), succIdx);
        }
    } else if (inst.getOpcode() == Instruction::PHI) {
        assert(pred && "Cannot handle PHIs without know predecessor");
        return handlePHINode(*dyn_cast<llvm::PHINode>(&inst), pred);
    }
    
    return transform(inst);
}

ExprPtr InstToExpr::transform(llvm::Instruction& inst)
{
    LLVM_DEBUG(llvm::dbgs() << "  Transforming instruction " << inst << "\n");
    #define HANDLE_INST(OPCODE, NAME)                                   \
        else if (inst.getOpcode() == (OPCODE)) {                        \
            return visit##NAME(*llvm::cast<llvm::NAME>(&inst));         \
        }                                                               \

    if (inst.isBinaryOp()) {
        return visitBinaryOperator(*dyn_cast<llvm::BinaryOperator>(&inst));
    } else if (inst.isCast()) {
        return visitCastInst(*dyn_cast<llvm::CastInst>(&inst));
    }
    HANDLE_INST(Instruction::ICmp, ICmpInst)
    HANDLE_INST(Instruction::Call, CallInst)
    HANDLE_INST(Instruction::FCmp, FCmpInst)
    HANDLE_INST(Instruction::Select, SelectInst)
    HANDLE_INST(Instruction::GetElementPtr, GetElementPtrInst)
    HANDLE_INST(Instruction::Load, LoadInst)
    HANDLE_INST(Instruction::Alloca, AllocaInst)
    HANDLE_INST(Instruction::Store, StoreInst)

    #undef HANDLE_INST

    assert(false && "Unsupported instruction kind");
}

//----- Basic instruction types -----//

ExprPtr InstToExpr::visitBinaryOperator(llvm::BinaryOperator &binop)
{
    auto variable = getVariable(&binop);
    auto lhs = operand(binop.getOperand(0));
    auto rhs = operand(binop.getOperand(1));

    auto opcode = binop.getOpcode();
    if (isLogicInstruction(opcode)) {
        ExprPtr expr;
        if (binop.getType()->isIntegerTy(1)) {
            auto boolLHS = asBool(lhs);
            auto boolRHS = asBool(rhs);

            if (binop.getOpcode() == Instruction::And) {
                expr = mExprBuilder->And(boolLHS, boolRHS);
            } else if (binop.getOpcode() == Instruction::Or) {
                expr = mExprBuilder->Or(boolLHS, boolRHS);
            } else if (binop.getOpcode() == Instruction::Xor) {
                expr = mExprBuilder->Xor(boolLHS, boolRHS);
            } else {
                llvm_unreachable("Unknown logic instruction opcode");
            }
        } else {
            assert(binop.getType()->isIntegerTy()
                && "Integer operations on non-integer types");
            auto iTy = llvm::dyn_cast<llvm::IntegerType>(binop.getType());

            auto intLHS = asInt(lhs, iTy->getBitWidth());
            auto intRHS = asInt(rhs, iTy->getBitWidth());

            if (binop.getOpcode() == Instruction::And) {
                expr = mExprBuilder->BAnd(intLHS, intRHS);
            } else if (binop.getOpcode() == Instruction::Or) {
                expr = mExprBuilder->BOr(intLHS, intRHS);
            } else if (binop.getOpcode() == Instruction::Xor) {
                expr = mExprBuilder->BXor(intLHS, intRHS);
            } else {
                llvm_unreachable("Unknown logic instruction opcode");
            }
        }

        return mExprBuilder->Eq(variable->getRefExpr(), expr);
    } else if (isFloatInstruction(opcode)) {
        ExprPtr expr;
        switch (binop.getOpcode()) {
            case Instruction::FAdd:
                expr = mExprBuilder->FAdd(lhs, rhs, llvm::APFloat::rmNearestTiesToEven);
                break;
            case Instruction::FSub:
                expr = mExprBuilder->FSub(lhs, rhs, llvm::APFloat::rmNearestTiesToEven);
                break;
            case Instruction::FMul:
                expr = mExprBuilder->FMul(lhs, rhs, llvm::APFloat::rmNearestTiesToEven);
                break;
            case Instruction::FDiv:
                expr = mExprBuilder->FDiv(lhs, rhs, llvm::APFloat::rmNearestTiesToEven);
                break;
            default:
                assert(false && "Invalid floating-point operation");
        }

        return mExprBuilder->FEq(variable->getRefExpr(), expr);
    } else {
        const BvType* type = llvm::dyn_cast<BvType>(&variable->getType());
        assert(type && "Arithmetic results must be integer types");

        auto intLHS = asInt(lhs, type->getWidth());
        auto intRHS = asInt(rhs, type->getWidth());

        #define HANDLE_INSTCASE(OPCODE, EXPRNAME)                 \
            case OPCODE:                                          \
                expr = mExprBuilder->EXPRNAME(intLHS, intRHS);    \
                break;                                            \

        ExprPtr expr;
        switch (binop.getOpcode()) {
            HANDLE_INSTCASE(Instruction::Add, Add)
            HANDLE_INSTCASE(Instruction::Sub, Sub)
            HANDLE_INSTCASE(Instruction::Mul, Mul)
            HANDLE_INSTCASE(Instruction::SDiv, SDiv)
            HANDLE_INSTCASE(Instruction::UDiv, UDiv)
            HANDLE_INSTCASE(Instruction::SRem, SRem)
            HANDLE_INSTCASE(Instruction::URem, URem)
            HANDLE_INSTCASE(Instruction::Shl, Shl)
            HANDLE_INSTCASE(Instruction::LShr, LShr)
            HANDLE_INSTCASE(Instruction::AShr, AShr)
            default:
                llvm::errs() << binop << "\n";
                assert(false && "Unsupported arithmetic instruction opcode");
        }

        #undef HANDLE_INSTCASE

        return mExprBuilder->Eq(variable->getRefExpr(), expr);
    }

    llvm_unreachable("Invalid binary operation kind");
}

ExprPtr InstToExpr::visitSelectInst(llvm::SelectInst& select)
{
    Variable* selectVar = getVariable(&select);
    const Type& type = selectVar->getType();

    auto cond = asBool(operand(select.getCondition()));
    auto then = castResult(operand(select.getTrueValue()), type);
    auto elze = castResult(operand(select.getFalseValue()), type);

    return mExprBuilder->Eq(selectVar->getRefExpr(), mExprBuilder->Select(cond, then, elze));
}

ExprPtr InstToExpr::visitICmpInst(llvm::ICmpInst& icmp)
{
    using llvm::CmpInst;
    
    auto icmpVar = getVariable(&icmp);
    auto lhs = operand(icmp.getOperand(0));
    auto rhs = operand(icmp.getOperand(1));

    auto pred = icmp.getPredicate();

    #define HANDLE_PREDICATE(PREDNAME, EXPRNAME)                \
        case PREDNAME:                                          \
            expr = mExprBuilder->EXPRNAME(lhs, rhs);            \
            break;                                              \

    ExprPtr expr;
    switch (pred) {
        HANDLE_PREDICATE(CmpInst::ICMP_EQ, Eq)
        HANDLE_PREDICATE(CmpInst::ICMP_NE, NotEq)
        HANDLE_PREDICATE(CmpInst::ICMP_UGT, UGt)
        HANDLE_PREDICATE(CmpInst::ICMP_UGE, UGtEq)
        HANDLE_PREDICATE(CmpInst::ICMP_ULT, ULt)
        HANDLE_PREDICATE(CmpInst::ICMP_ULE, ULtEq)
        HANDLE_PREDICATE(CmpInst::ICMP_SGT, SGt)
        HANDLE_PREDICATE(CmpInst::ICMP_SGE, SGtEq)
        HANDLE_PREDICATE(CmpInst::ICMP_SLT, SLt)
        HANDLE_PREDICATE(CmpInst::ICMP_SLE, SLtEq)
        default:
            assert(false && "Unhandled ICMP predicate.");
    }

    #undef HANDLE_PREDICATE

    return mExprBuilder->Eq(icmpVar->getRefExpr(), expr);
}

ExprPtr InstToExpr::visitFCmpInst(llvm::FCmpInst& fcmp)
{
    using llvm::CmpInst;
    
    auto fcmpVar = getVariable(&fcmp);
    auto left = operand(fcmp.getOperand(0));
    auto right = operand(fcmp.getOperand(1));

    auto pred = fcmp.getPredicate();

    ExprPtr cmpExpr = nullptr;
    switch (pred) {
        case CmpInst::FCMP_OEQ:
        case CmpInst::FCMP_UEQ:
            cmpExpr = mExprBuilder->FEq(left, right);
            break;  
        case CmpInst::FCMP_OGT:
        case CmpInst::FCMP_UGT:
            cmpExpr = mExprBuilder->FGt(left, right);
            break;  
        case CmpInst::FCMP_OGE:
        case CmpInst::FCMP_UGE:
            cmpExpr = mExprBuilder->FGtEq(left, right);
            break;  
        case CmpInst::FCMP_OLT:
        case CmpInst::FCMP_ULT:
            cmpExpr = mExprBuilder->FLt(left, right);
            break;  
        case CmpInst::FCMP_OLE:
        case CmpInst::FCMP_ULE:
            cmpExpr = mExprBuilder->FLtEq(left, right);
            break;  
        case CmpInst::FCMP_ONE:
        case CmpInst::FCMP_UNE:
            cmpExpr = mExprBuilder->Not(mExprBuilder->FEq(left, right));
            break;
        default:
            break;
    }


    ExprPtr expr = nullptr;
    if (pred == CmpInst::FCMP_FALSE) {
        expr = mExprBuilder->False();
    } else if (pred == CmpInst::FCMP_TRUE) {
        expr = mExprBuilder->True();
    } else if (AssumeNoNaN) {
        if (pred == CmpInst::FCMP_ORD) {
            expr = mExprBuilder->True();
        } else if (pred == CmpInst::FCMP_UNO) {
            expr = mExprBuilder->False();
        } else {
            expr = cmpExpr;
        }
    } else if (pred == CmpInst::FCMP_ORD) {
        expr = mExprBuilder->And(
            mExprBuilder->Not(mExprBuilder->FIsNan(left)),
            mExprBuilder->Not(mExprBuilder->FIsNan(right))
        );
    } else if (pred == CmpInst::FCMP_UNO) {
        expr = mExprBuilder->Or(
            mExprBuilder->FIsNan(left),
            mExprBuilder->FIsNan(right)
        );
    } else if (CmpInst::isOrdered(pred)) {
        // An ordered instruction can only be true if it has no NaN operands.
        expr = mExprBuilder->And({
            mExprBuilder->Not(mExprBuilder->FIsNan(left)),
            mExprBuilder->Not(mExprBuilder->FIsNan(right)),
            cmpExpr
        });
    } else if (CmpInst::isUnordered(pred)) {
        // An unordered instruction may be true if either operand is NaN
        expr = mExprBuilder->Or({
            mExprBuilder->FIsNan(left),
            mExprBuilder->FIsNan(right),
            cmpExpr
        });
    } else {
        llvm_unreachable("Invalid FCmp predicate");
    }

    return mExprBuilder->Eq(fcmpVar->getRefExpr(), expr);
}

ExprPtr InstToExpr::integerCast(llvm::CastInst& cast, ExprPtr operand, unsigned width)
{
    auto variable = getVariable(&cast);
    auto intTy = llvm::dyn_cast<gazer::BvType>(&variable->getType());
    
    ExprPtr intOp = asInt(operand, width);
    ExprPtr castOp = nullptr;
    if (cast.getOpcode() == Instruction::ZExt) {
        castOp = mExprBuilder->ZExt(intOp, *intTy);
    } else if (cast.getOpcode() == Instruction::SExt) {
        castOp = mExprBuilder->SExt(intOp, *intTy);
    } else if (cast.getOpcode() == Instruction::Trunc) {
        castOp = mExprBuilder->Trunc(intOp, *intTy);
    } else {
        llvm_unreachable("Unhandled integer cast operation");
    }

    return mExprBuilder->Eq(variable->getRefExpr(), castOp);
}

ExprPtr InstToExpr::visitCastInst(llvm::CastInst& cast)
{
    auto castOp = operand(cast.getOperand(0));
    if (cast.getOperand(0)->getType()->isPointerTy()) {
        return mMemoryModel.handlePointerCast(cast, castOp);
    }

    if (castOp->getType().isBoolType()) {
        return integerCast(cast, castOp, 1);
    } else if (castOp->getType().isBvType()) {
        return integerCast(
            cast, castOp, dyn_cast<BvType>(&castOp->getType())->getWidth()
        );
    }

    assert(false && "Unsupported cast operation");
}

ExprPtr InstToExpr::visitCallInst(llvm::CallInst& call)
{
    const Function* callee = call.getCalledFunction();
    if (callee == nullptr) {
        // This is an indirect call, use the memory model to resolve it.
        return mMemoryModel.handleCall(call);
    }

    assert(callee != nullptr && "Indirect calls are not supported.");

    if (callee->isDeclaration()) {
        if (callee->getName() == "gazer.malloc") {
            auto siz = call.getArgOperand(0);
            auto start = call.getArgOperand(1);
        } else if (call.getName() != "") {
            // This is not a known function,
            // we just replace the call with an undef value
            auto variable = getVariable(&call);
            
            return mExprBuilder->True();
            //return mExprBuilder->Eq(variable->getRefExpr(), UndefExpr::Get(variable->getType()));
        }
    } else {
        assert(false && "Procedure calls are not supported (with the exception of assert).");
    }

    // XXX: This a temporary hack for no-op instructions
    return mExprBuilder->True();
}

//----- Branches and PHI nodes -----//
ExprPtr InstToExpr::handlePHINode(llvm::PHINode& phi, BasicBlock* pred)
{    
    auto variable = getVariable(&phi);
    auto expr = operand(phi.getIncomingValueForBlock(pred));

    return mExprBuilder->Eq(variable->getRefExpr(), expr);
}

ExprPtr InstToExpr::handleBr(llvm::BranchInst& br, size_t succIdx)
{
    assert((succIdx == 0 || succIdx == 1)
        && "Invalid successor index for Br");
    
    if (br.isUnconditional()) {
        return mExprBuilder->True();
    }

    auto cond = asBool(operand(br.getCondition()));

    if (succIdx == 0) {
        // This is the 'true' path
        return cond;
    } else {
        return mExprBuilder->Not(cond);
    }
}


ExprPtr InstToExpr::handleSwitch(llvm::SwitchInst& swi, size_t succIdx)
{
    ExprPtr condition = operand(swi.getCondition());
    
    llvm::SwitchInst::CaseIt caseIt = swi.case_begin();

    if (succIdx == 0) {
        // The SwitchInst is taking the default branch
        ExprPtr expr = mExprBuilder->True();
        while (caseIt != swi.case_end()) {
            if (caseIt != swi.case_default()) {
                ExprPtr value = operand(caseIt->getCaseValue());

                // A folding ExprBuilder will optimize this
                expr = mExprBuilder->And(
                    expr,
                    mExprBuilder->NotEq(condition, value)
                );
            }

            ++caseIt;
        }

        return expr;
    }

    // A normal case branch is taken
    while (caseIt != swi.case_end()) {
        if (caseIt->getSuccessorIndex() == succIdx) {
            break;
        }

        ++caseIt;
    }

    assert(caseIt != swi.case_end()
        && "The successor should be present in the SwitchInst");

    ExprPtr value = operand(caseIt->getCaseValue());

    return mExprBuilder->Eq(condition, value);
}

ExprPtr InstToExpr::visitLoadInst(llvm::LoadInst& load)
{
    return mMemoryModel.handleLoad(load);
}

ExprPtr InstToExpr::visitStoreInst(llvm::StoreInst& store)
{
    return mMemoryModel.handleStore(store);
}

ExprPtr InstToExpr::visitAllocaInst(llvm::AllocaInst& alloca)
{
    return mMemoryModel.handleAlloca(alloca);
}

ExprPtr InstToExpr::visitGetElementPtrInst(llvm::GetElementPtrInst& gep)
{
    ExprVector exprs;
    std::transform(
        gep.op_begin(), gep.op_end(),
        std::back_inserter(exprs),
        [this] (const llvm::Value* val) { return operand(val); }
    );

    return mMemoryModel.handleGetElementPtr(gep, exprs);
}

//----- Utils and casting -----//

ExprPtr InstToExpr::operand(const Value* value)
{
    if (const ConstantInt* ci = dyn_cast<ConstantInt>(value)) {
        // Check for boolean literals
        if (ci->getType()->isIntegerTy(1)) {
            return ci->isZero() ? mExprBuilder->False() : mExprBuilder->True();
        }

        return mExprBuilder->BvLit(
            ci->getValue().getLimitedValue(),
            ci->getType()->getIntegerBitWidth()
        );
    } else if (const llvm::ConstantFP* cfp = dyn_cast<llvm::ConstantFP>(value)) {
        return FloatLiteralExpr::Get(
            *llvm::dyn_cast<FloatType>(&typeFromLLVMType(cfp->getType())),
            cfp->getValueAPF()
        );
    } else if (const llvm::ConstantPointerNull* ptr = dyn_cast<llvm::ConstantPointerNull>(value)) {
        return mMemoryModel.getNullPointer();
    } else if (isNonConstValue(value)) {
        auto result = mEliminatedValues.find(value);
        if (result != mEliminatedValues.end()) {
            return result->second;
        }

        return getVariable(value)->getRefExpr();
    } else if (isa<llvm::UndefValue>(value)) {
        return mExprBuilder->Undef(typeFromLLVMType(value));
    } else {
        LLVM_DEBUG(llvm::dbgs() << "  Unhandled value for operand: " << *value << "\n");
        assert(false && "Unhandled value type");
    }
}

Variable* InstToExpr::getVariable(const Value* value)
{
    auto result = mVariables.find(value);
    assert(result != mVariables.end() && "Variables should be present in the variable map.");

    return result->second;
}

ExprPtr InstToExpr::asBool(ExprPtr operand)
{
    if (operand->getType().isBoolType()) {
        return operand;
    } else if (operand->getType().isBvType()) {
        const BvType* bvTy = dyn_cast<BvType>(&operand->getType());
        unsigned bits = bvTy->getWidth();

        return mExprBuilder->Select(
            mExprBuilder->Eq(operand, mExprBuilder->BvLit(0, bits)),
            mExprBuilder->False(),
            mExprBuilder->True()
        );
    } else {
        assert(false && "Unsupported gazer type.");
    }
}

ExprPtr InstToExpr::asInt(ExprPtr operand, unsigned bits)
{
    if (operand->getType().isBoolType()) {
        return mExprBuilder->Select(
            operand,
            mExprBuilder->BvLit(1, bits),
            mExprBuilder->BvLit(0, bits)
        );
    } else if (operand->getType().isBvType()) {
        return operand;
    } else {
        assert(false && "Unsupported gazer type.");
    }
}

ExprPtr InstToExpr::castResult(ExprPtr expr, const Type& type)
{
    if (type.isBoolType()) {
        return asBool(expr);
    } else if (type.isBvType()) {
        return asInt(expr, dyn_cast<BvType>(&type)->getWidth());
    } else {
        assert(!"Invalid cast result type");
    }
}
