#include "gazer/Core/Utils/ExprBuilder.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/Variable.h"

#include <llvm/ADT/DenseMap.h>

using namespace gazer;

namespace
{

class FoldingExprBuilder : public ExprBuilder
{
public:
    FoldingExprBuilder()
        : ExprBuilder()
    {}

    ExprPtr Not(const ExprPtr& op) override {
        if (op->getKind() == Expr::Literal) {
            auto lit = llvm::dyn_cast<BoolLiteralExpr>(op.get());
            if (lit->getValue() == true) {
                return BoolLiteralExpr::getFalse();
            } else {
                return BoolLiteralExpr::getTrue();
            }
        }

        return NotExpr::Create(op);
    }

    ExprPtr ZExt(const ExprPtr& op, const IntType& type) override {
        return ZExtExpr::Create(op, type);
    }
    ExprPtr SExt(const ExprPtr& op, const IntType& type) override {
        return SExtExpr::Create(op, type);
    }
    ExprPtr Trunc(const ExprPtr& op, const IntType& type) override {
        return ExtractExpr::Create(op, 0, type.getWidth());
    }
    ExprPtr Extract(const ExprPtr& op, unsigned offset, unsigned width) override {
        return ExtractExpr::Create(op, offset, width);
    }
    ExprPtr PtrCast(const ExprPtr& op, const PointerType& type) override {
        return PtrCastExpr::Create(op, type);
    }

    ExprPtr Add(const ExprPtr& left, const ExprPtr& right) override {
        return AddExpr::Create(left, right);
    }
    ExprPtr Sub(const ExprPtr& left, const ExprPtr& right) override {
        return SubExpr::Create(left, right);
    }
    ExprPtr Mul(const ExprPtr& left, const ExprPtr& right) override {
        return MulExpr::Create(left, right);
    }

    ExprPtr SDiv(const ExprPtr& left, const ExprPtr& right) override {
        return SDivExpr::Create(left, right);
    }
    ExprPtr UDiv(const ExprPtr& left, const ExprPtr& right) override {
        return UDivExpr::Create(left, right);
    }
    ExprPtr SRem(const ExprPtr& left, const ExprPtr& right) override {
        return SRemExpr::Create(left, right);
    }
    ExprPtr URem(const ExprPtr& left, const ExprPtr& right) override {
        return URemExpr::Create(left, right);
    }

    ExprPtr Shl(const ExprPtr& left, const ExprPtr& right) override {
        return ShlExpr::Create(left, right);
    }
    ExprPtr LShr(const ExprPtr& left, const ExprPtr& right) override {
        return LShrExpr::Create(left, right);
    }
    ExprPtr AShr(const ExprPtr& left, const ExprPtr& right) override {
        return AShrExpr::Create(left, right);
    }
    ExprPtr BAnd(const ExprPtr& left, const ExprPtr& right) override {
        return BAndExpr::Create(left, right);
    }
    ExprPtr BOr(const ExprPtr& left, const ExprPtr& right) override {
        return BOrExpr::Create(left, right);
    }
    ExprPtr BXor(const ExprPtr& left, const ExprPtr& right) override {
        return BXorExpr::Create(left, right);
    }

    ExprPtr And(const ExprVector& vector) override {
        ExprVector newOps;

        for (const ExprPtr& op : vector) {
            if (op->getKind() == Expr::Literal) {
                auto lit = llvm::dyn_cast<BoolLiteralExpr>(op.get());
                if (lit->getValue() == false) {
                    return BoolLiteralExpr::getFalse();
                } else {
                    // We are not adding unnecessary true literals
                }
            } else if (op->getKind() == Expr::And) {
                // For AndExpr operands, we try to flatten the expression
                auto andExpr = llvm::dyn_cast<AndExpr>(op.get());    
                newOps.insert(newOps.end(), andExpr->op_begin(), andExpr->op_end());
            } else {
                newOps.push_back(op);
            }
        }

        if (newOps.size() == 0) {
            // If we eliminated all operands
            return BoolLiteralExpr::getTrue();
        } else if (newOps.size() == 1) {
            return *newOps.begin();
        }

        return AndExpr::Create(newOps.begin(), newOps.end());
    }

    ExprPtr Or(const ExprVector& vector) override {
        ExprVector newOps;

        for (const ExprPtr& op : vector) {
            if (op->getKind() == Expr::Literal) {
                auto lit = llvm::dyn_cast<BoolLiteralExpr>(op.get());
                if (lit->getValue() == true) {
                    return BoolLiteralExpr::getTrue();
                } else {
                    // We are not adding unnecessary false literals
                }
            } else if (op->getKind() == Expr::Or) {
                // For OrExpr operands, we try to flatten the expression
                auto orExpr = llvm::dyn_cast<OrExpr>(op.get());    
                newOps.insert(newOps.end(), orExpr->op_begin(), orExpr->op_end());
            } else {
                newOps.push_back(op);
            }
        }

        if (newOps.size() == 0) {
            // If we eliminated all operands
            return BoolLiteralExpr::getFalse();
        } else if (newOps.size() == 1) {
            return *newOps.begin();
        }

        // Try some optimizations for the binary case
        if (newOps.size() == 2) {
            if (newOps[0]->getKind() == Expr::And && newOps[1]->getKind() == Expr::And) {
                // (F1 & F2) | (F1 & F3) => F1 & (F1 | F3)
            }
        }
       
        return OrExpr::Create(newOps.begin(), newOps.end());
    }

    ExprPtr Xor(const ExprPtr& left, const ExprPtr& right) override
    {
        if (left == BoolLiteralExpr::getTrue()) {
            return this->Not(right);
        } else if (right == BoolLiteralExpr::getFalse()) {
            return this->Not(left);
        } else if (left == BoolLiteralExpr::getFalse()) {
            return right;
        } else if (right == BoolLiteralExpr::getFalse()) {
            return left;
        }

        return XorExpr::Create(left, right);
    }

    ExprPtr Eq(const ExprPtr& left, const ExprPtr& right) override {
        if (left == right) {
            return BoolLiteralExpr::getTrue();
        }

        if (left->getKind() == Expr::Literal && right->getKind() == Expr::Literal) {
            if (left->getType().isIntType()) {
                assert(right->getType().isIntType() && "EqExpr operand types should match");
                auto lhsLit = llvm::dyn_cast<IntLiteralExpr>(left.get());
                auto rhsLit = llvm::dyn_cast<IntLiteralExpr>(right.get());

                return BoolLiteralExpr::Get(lhsLit->getValue() == rhsLit->getValue());
            }
        } else if (left->getKind() == Expr::VarRef && right->getKind() == Expr::VarRef) {
            auto lhsVar = &(llvm::dyn_cast<VarRefExpr>(left.get())->getVariable());
            auto rhsVar = &(llvm::dyn_cast<VarRefExpr>(right.get())->getVariable());

            if (lhsVar == rhsVar) {
                return BoolLiteralExpr::getTrue();
            }
        }

        return EqExpr::Create(left, right);
    }

    ExprPtr NotEq(const ExprPtr& left, const ExprPtr& right) override {
        if (left == right) {
            return BoolLiteralExpr::getFalse();
        }

        if (left->getKind() == Expr::Literal && right->getKind() == Expr::Literal) {
            if (left->getType().isIntType()) {
                assert(right->getType().isIntType() && "EqExpr operand types should match");
                auto lhsLit = llvm::dyn_cast<IntLiteralExpr>(left.get());
                auto rhsLit = llvm::dyn_cast<IntLiteralExpr>(right.get());

                return BoolLiteralExpr::Get(lhsLit->getValue() != rhsLit->getValue());
            }
        } else if (left->getKind() == Expr::VarRef && right->getKind() == Expr::VarRef) {
            auto lhsVar = &(llvm::dyn_cast<VarRefExpr>(left.get())->getVariable());
            auto rhsVar = &(llvm::dyn_cast<VarRefExpr>(right.get())->getVariable());

            if (lhsVar == rhsVar) {
                return BoolLiteralExpr::getFalse();
            }
        }
        
        return NotEqExpr::Create(left, right);
    }

    ExprPtr SLt(const ExprPtr& left, const ExprPtr& right) override {
        return SLtExpr::Create(left, right);
    }
    ExprPtr SLtEq(const ExprPtr& left, const ExprPtr& right) override {
        return SLtEqExpr::Create(left, right);
    }
    ExprPtr SGt(const ExprPtr& left, const ExprPtr& right) override {
        return SGtExpr::Create(left, right);
    }
    ExprPtr SGtEq(const ExprPtr& left, const ExprPtr& right) override {
        return SGtEqExpr::Create(left, right);
    }

    ExprPtr ULt(const ExprPtr& left, const ExprPtr& right) override {
        return ULtExpr::Create(left, right);
    }
    ExprPtr ULtEq(const ExprPtr& left, const ExprPtr& right) override {
        return ULtEqExpr::Create(left, right);
    }
    ExprPtr UGt(const ExprPtr& left, const ExprPtr& right) override {
        return UGtExpr::Create(left, right);
    }
    ExprPtr UGtEq(const ExprPtr& left, const ExprPtr& right) override {
        return UGtEqExpr::Create(left, right);
    }

    ExprPtr FIsNan(const ExprPtr& op) override {
        if (op->getKind() == Expr::Literal) {
            auto fltLit = llvm::dyn_cast<FloatLiteralExpr>(op.get());
            return BoolLiteralExpr::Get(fltLit->getValue().isNaN());
        }

        return FIsNanExpr::Create(op);
    }
    ExprPtr FIsInf(const ExprPtr& op) override {
        if (op->getKind() == Expr::Literal) {
            auto fltLit = llvm::dyn_cast<FloatLiteralExpr>(op.get());
            return BoolLiteralExpr::Get(fltLit->getValue().isInfinity());
        }

        return FIsInfExpr::Create(op);
    }
    
    ExprPtr FAdd(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) override {
        return FAddExpr::Create(left, right, rm);
    }
    ExprPtr FSub(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) override {
        return FSubExpr::Create(left, right, rm);
    }
    ExprPtr FMul(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) override {
        return FMulExpr::Create(left, right, rm);
    }
    ExprPtr FDiv(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) override {
        if (left->getKind() == Expr::Literal && right->getKind() == Expr::Literal) {
            auto fltLeft  = llvm::dyn_cast<FloatLiteralExpr>(left.get());
            auto fltRight = llvm::dyn_cast<FloatLiteralExpr>(right.get());

            llvm::APFloat result(fltLeft->getValue());
            result.divide(fltRight->getValue(), rm);

            return FloatLiteralExpr::get(
                *llvm::dyn_cast<FloatType>(&left->getType()), result
            );
        }

        return FDivExpr::Create(left, right, rm);
    }
    
    ExprPtr FEq(const ExprPtr& left, const ExprPtr& right) override {
        return FEqExpr::Create(left, right);
    }
    ExprPtr FGt(const ExprPtr& left, const ExprPtr& right) override {
        return FGtExpr::Create(left, right);
    }
    ExprPtr FGtEq(const ExprPtr& left, const ExprPtr& right) override {
        return FGtEqExpr::Create(left, right);
    }
    ExprPtr FLt(const ExprPtr& left, const ExprPtr& right) override {
        return FLtExpr::Create(left, right);
    }
    ExprPtr FLtEq(const ExprPtr& left, const ExprPtr& right) override {
        return FLtEqExpr::Create(left, right);
    }

    ExprPtr Select(const ExprPtr& condition, const ExprPtr& then, const ExprPtr& elze) override {
        if (condition->getKind() == Expr::Literal) {
            auto lit = llvm::dyn_cast<BoolLiteralExpr>(condition.get());
            if (lit->getValue() == true) {
                return then;
            } else {
                return elze;
            }
        }

        return SelectExpr::Create(condition, then, elze);
    }
};

} // end anonymous namespace

std::unique_ptr<ExprBuilder> gazer::CreateFoldingExprBuilder() {
    return std::unique_ptr<ExprBuilder>(new FoldingExprBuilder());
}