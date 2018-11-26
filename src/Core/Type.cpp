#include "gazer/Core/Type.h"

#include <fmt/format.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/DenseMap.h>

#include <algorithm>

using namespace gazer;

static std::string getArrayTypeStr(Type* indexType, Type* elemType)
{
    return fmt::format("[{0} -> {1}]",
        indexType->getName(), elemType->getName());
}

static std::string getFunctionTypeStr(Type* returnType, llvm::iterator_range<FunctionType::arg_iterator> args) {
    auto argPrint = [](Type* type) { return type->getName(); };
    auto range = llvm::make_range(
        llvm::map_iterator(args.begin(), argPrint),
        llvm::map_iterator(args.end(), argPrint)
    );

    return fmt::format("({0}) -> {1}",
        fmt::join(range, ", "), returnType->getName()
    );
}

TypeCastError::TypeCastError(const Type& from, const Type& to, std::string message)
    : TypeCastError(&from, &to, message)
{}

TypeCastError::TypeCastError(const Type* from, const Type* to, std::string message)
    : logic_error(fmt::format(
        "TypeCastError encountered: Invalid cast from type '{0}' to {1}."
        "Error message: {2}",
        from->getName(), to->getName(), message
    ))
{}

TypeCastError::TypeCastError(std::string message)
    : logic_error(message)
{}

std::string Type::getName() const
{
    switch (getTypeID()) {
        case BoolTypeID:
            return "Bool";
        case BvTypeID: {
            auto intType = llvm::cast<BvType>(this);
            return "Bv" + std::to_string(intType->getWidth());
        }
        case MathIntTypeID:
            return "Int";
        case FloatTypeID: {
            auto fltTy = llvm::cast<FloatType>(this);
            return "Float" + std::to_string(fltTy->getWidth());
        }
        case ArrayTypeID: {
            auto arrayType = llvm::cast<ArrayType>(this);
            return getArrayTypeStr(
                arrayType->getIndexType(), arrayType->getElementType()
            );
        }
        case FunctionTypeID: {
            auto funcType = llvm::cast<FunctionType>(this);
            return getFunctionTypeStr(
                funcType->getReturnType(), funcType->args()
            );
        }
    }

    llvm_unreachable("Invalid TypeID");
}

bool Type::equals(const Type* other) const
{
    if (getTypeID() != other->getTypeID()) {
        return false;
    } else if (getTypeID() == BvTypeID) {
        auto left = llvm::dyn_cast<BvType>(this);
        auto right = llvm::dyn_cast<BvType>(other);

        return left->getWidth() == right->getWidth();
    } else if (getTypeID() == FloatTypeID) {
        auto left = llvm::dyn_cast<FloatType>(this);
        auto right = llvm::dyn_cast<FloatType>(other);

        return left->getPrecision() == right->getPrecision();
    } else if (getTypeID() == ArrayTypeID) {
        auto left = llvm::dyn_cast<ArrayType>(this);
        auto right = llvm::dyn_cast<ArrayType>(other);

        return left->getIndexType() == right->getIndexType()
            && left->getElementType() == right->getElementType();
    } else if (getTypeID() == FunctionTypeID) {
        auto left = llvm::dyn_cast<FunctionType>(this);
        auto right = llvm::dyn_cast<FunctionType>(other);

        if (left->getReturnType() != right->getReturnType()) {
            return false;
        }

        return std::equal(
            left->arg_begin(), left->arg_end(),
            right->arg_begin(), right->arg_end(), [](auto lt, auto rt) {
                return lt->equals(rt);
            }
        );
    }

    return true;
}

BvType BvType::Int1Ty(1);
BvType BvType::Int8Ty(8);
BvType BvType::Int16Ty(16);
BvType BvType::Int32Ty(32);
BvType BvType::Int64Ty(64);

BvType* BvType::get(unsigned width)
{
    static llvm::DenseMap<unsigned, std::unique_ptr<BvType>> Instances;

    switch (width) {
        case 1: return &Int1Ty;
        case 8: return &Int8Ty;
        case 16: return &Int16Ty;
        case 32: return &Int32Ty;
        case 64: return &Int64Ty;
    }

    auto result = Instances.find(width);
    if (result == Instances.end()) {
        auto pair = Instances.try_emplace(width, new BvType(width));
        return pair.first->second.get();
    }

    return result->second.get();
}

FloatType FloatType::HalfTy(Half);
FloatType FloatType::SingleTy(Single);
FloatType FloatType::DoubleTy(Double);
FloatType FloatType::QuadTy(Quad);

FloatType* FloatType::get(FloatType::FloatPrecision precision)
{
    switch (precision) {
        case Half: return &HalfTy;
        case Single: return &SingleTy;
        case Double: return &DoubleTy;
        case Quad: return &QuadTy;
    }

    llvm_unreachable("Invalid floating-point type");
}

const llvm::fltSemantics& FloatType::getLLVMSemantics() const
{
    switch (getPrecision()) {
        case Half: return llvm::APFloat::IEEEhalf();
        case Single: return llvm::APFloat::IEEEsingle();
        case Double: return llvm::APFloat::IEEEdouble();
        case Quad: return llvm::APFloat::IEEEquad();
    }

    llvm_unreachable("Invalid floating-point type");
}

ArrayType* ArrayType::get(Type* indexType, Type* elementType)
{
    assert(indexType != nullptr);
    assert(elementType != nullptr);
    // TODO: This is surely not the best way to do this
    static llvm::StringMap<std::unique_ptr<ArrayType>> Instances;
    
    auto key = getArrayTypeStr(indexType, elementType);
    auto result = Instances.find(key);

    if (result == Instances.end()) {
        auto pair = Instances.try_emplace(key, new ArrayType(indexType, elementType));
        return pair.first->second.get();
    }

    return result->second.get();
}

FunctionType* FunctionType::get(Type* returnType, std::vector<Type*> args)
{
    static llvm::StringMap<std::unique_ptr<FunctionType>> Instances;

    // TODO: This is surely not the best way to do this
    auto key = getFunctionTypeStr(returnType, llvm::make_range(args.begin(), args.end()));

    auto result = Instances.find(key);
    if (result == Instances.end()) {
        auto pair = Instances.try_emplace(key, new FunctionType(returnType, args));
        return pair.first->second.get();
    }

    return result->second.get();
}
