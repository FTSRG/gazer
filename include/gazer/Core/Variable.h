#ifndef _GAZER_CORE_VARIABLE_H
#define _GAZER_CORE_VARIABLE_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/Symbol.h"
#include "gazer/Core/GazerContext.h"

#include <string>
#include <memory>

// TODO: Move this header into Expr or ExprTypes?

namespace gazer
{

class VarRefExpr;

class Variable final
{
    friend class GazerContext;
    friend class GazerContextImpl;

    Variable(llvm::StringRef name, Type& type);
public:
    Variable(const Variable&) = delete;
    Variable& operator=(const Variable&) = delete;

    bool operator==(const Variable& other) const;
    bool operator!=(const Variable& other) const { return !operator==(other); }

    std::string getName() const { return mName; }
    Type& getType() const { return mType; }
    ExprRef<VarRefExpr> getRefExpr() const { return mExpr; }

    GazerContext& getContext() const { return mType.getContext(); }

private:
    std::string mName;
    Type& mType;
    ExprRef<VarRefExpr> mExpr;
};

class VarRefExpr final : public Expr
{
    friend class Variable;
    friend class ExprStorage;
private:
    VarRefExpr(Variable* variable);

public:
    Variable& getVariable() const { return *mVariable; }

    virtual void print(llvm::raw_ostream& os) const override;

    static bool classof(const Expr* expr) {
        return expr->getKind() == Expr::VarRef;
    }

private:
    Variable* mVariable;
};

/// Convenience class for representing assignments to variables.
class VariableAssignment final
{
public:
    VariableAssignment(Variable *variable, ExprPtr value)
        : mVariable(variable), mValue(value)
    {
        assert(variable->getType() == value->getType());
    }

    Variable* getVariable() const { return mVariable; }
    ExprPtr getValue() const { return mValue; }

    void print(llvm::raw_ostream& os) const;

private:
    Variable* mVariable;
    ExprPtr mValue;
};


llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const Variable& variable);
llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const VariableAssignment& va);

} // end namespace gazer

#endif
