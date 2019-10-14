#ifndef GAZER_LLVM_INSTTOEXPR_H
#define GAZER_LLVM_INSTTOEXPR_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/LLVM/Memory/MemoryObject.h"

#include <llvm/IR/Operator.h>
#include <llvm/IR/Instructions.h>

namespace gazer
{

/// A transformation class which may be used to transform LLVM instructions
/// to gazer expressions.
class InstToExpr
{
public:
    InstToExpr(
        ExprBuilder& builder,
        MemoryModel& memoryModel,
        LLVMFrontendSettings settings
    ) : mExprBuilder(builder),
        mContext(builder.getContext()),
        mMemoryModel(memoryModel),
        mSettings(settings)
    {}

    ExprPtr transform(const llvm::Instruction& inst);

    virtual ~InstToExpr() = default;

protected:
    virtual Variable* getVariable(const llvm::Value* value) = 0;

    /// If \p value was inlined, returns the corresponding expression.
    /// Otherwise, this method should return nullptr.
    virtual ExprPtr lookupInlinedVariable(const llvm::Value* value) {
        return nullptr;
    }

protected:
    ExprPtr visitBinaryOperator(const llvm::BinaryOperator& binop);
    ExprPtr visitSelectInst(const llvm::SelectInst& select);
    ExprPtr visitICmpInst(const llvm::ICmpInst& icmp);
    ExprPtr visitFCmpInst(const llvm::FCmpInst& fcmp);
    ExprPtr visitCastInst(const llvm::CastInst& cast);
    ExprPtr visitCallInst(const llvm::CallInst& call);
    ExprPtr visitLoadInst(const llvm::LoadInst& load);
    ExprPtr visitGEPOperator(const llvm::GEPOperator& gep);
    ExprPtr visitAllocaInst(const llvm::AllocaInst& alloc);

    ExprPtr operand(const llvm::Value* value);
    
    ExprPtr asBool(const ExprPtr& operand);
    ExprPtr asBv(const ExprPtr& operand, unsigned int bits);
    ExprPtr asInt(const ExprPtr& operand);

    ExprPtr integerCast(const llvm::CastInst& cast, const ExprPtr& operand, unsigned int width);
    ExprPtr castResult(const ExprPtr& expr, const Type& type);
    ExprPtr boolToIntCast(const llvm::CastInst& cast, const ExprPtr& operand);

    gazer::Type& translateType(const llvm::Type* type);

    template<class Ty>
    Ty& translateTypeTo(const llvm::Type* type)
    {
        gazer::Type& gazerTy = this->translateType(type);
        assert(llvm::isa<Ty>(&gazerTy));

        return *llvm::cast<Ty>(&gazerTy);
    }
private:
    ExprPtr unsignedLessThan(const ExprPtr& left, const ExprPtr& right);

protected:
    ExprBuilder& mExprBuilder;
    GazerContext& mContext;
    MemoryModel& mMemoryModel;
    LLVMFrontendSettings mSettings;
};

} // end namespace gazer

#endif
