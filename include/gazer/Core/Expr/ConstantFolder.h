#ifndef _GAZER_CORE_EXPR_CONSTANTFOLDER_H
#define _GAZER_CORE_EXPR_CONSTANTFOLDER_H

#include "gazer/Core/Expr.h"

#include <llvm/ADT/APFloat.h>

namespace gazer
{

class ConstantFolder
{
public:
    static ExprPtr Not(const ExprPtr& op);
    static ExprPtr ZExt(const ExprPtr& op, BvType& type);
    static ExprPtr SExt(const ExprPtr& op, BvType& type);
    static ExprPtr Trunc(const ExprPtr& op, BvType& type);
    static ExprPtr Extract(const ExprPtr& op, unsigned offset, unsigned width);

    //--- Binary ---//
    static ExprPtr Add(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr Sub(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr Mul(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr BvSDiv(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr BvUDiv(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr BvSRem(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr BvURem(const ExprPtr& left, const ExprPtr& right);

    static ExprPtr Shl(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr LShr(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr AShr(const ExprPtr& left, const ExprPtr& right);    
    static ExprPtr BvAnd(const ExprPtr& left, const ExprPtr& right);    
    static ExprPtr BvOr(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr BvXor(const ExprPtr& left, const ExprPtr& right);

    static ExprPtr And(const ExprVector& vector);
    static ExprPtr Or(const ExprVector& vector);

    static ExprPtr And(const ExprPtr& left, const ExprPtr& right) {
        return And({left, right});
    }
    static ExprPtr Or(const ExprPtr& left, const ExprPtr& right) {
        return Or({left, right});
    }

    template<class InputIterator>
    ExprPtr And(InputIterator begin, InputIterator end) {
        return And(ExprVector(begin, end));
    }
    template<class InputIterator>
    ExprPtr Or(InputIterator begin, InputIterator end) {
        return Or(ExprVector(begin, end));
    }

    static ExprPtr Xor(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr Imply(const ExprPtr& left, const ExprPtr& right);

    static ExprPtr Eq(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr NotEq(const ExprPtr& left, const ExprPtr& right);

    static ExprPtr BvSLt(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr BvSLtEq(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr BvSGt(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr BvSGtEq(const ExprPtr& left, const ExprPtr& right);

    static ExprPtr BvULt(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr BvULtEq(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr BvUGt(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr BvUGtEq(const ExprPtr& left, const ExprPtr& right);

    //--- Floating point ---//
    static ExprPtr FIsNan(const ExprPtr& op);
    static ExprPtr FIsInf(const ExprPtr& op);
    
    static ExprPtr FAdd(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm);
    static ExprPtr FSub(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm);
    static ExprPtr FMul(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm);
    static ExprPtr FDiv(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm);
    
    static ExprPtr FEq(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr FGt(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr FGtEq(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr FLt(const ExprPtr& left, const ExprPtr& right);
    static ExprPtr FLtEq(const ExprPtr& left, const ExprPtr& right);

    //--- Ternary ---//
    static ExprPtr Select(const ExprPtr& condition, const ExprPtr& then, const ExprPtr& elze);
};

}

#endif