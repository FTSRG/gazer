#ifndef _GAZER_TRACE_SAFETYRESULT_H
#define _GAZER_TRACE_SAFETYRESULT_H

#include "gazer/Trace/Trace.h"

namespace gazer
{

class SafetyResult
{
public:
    enum Status { Success, Fail, Unknown, BoundReached, InternalError };
protected:
    SafetyResult(Status status)
        : mStatus(status)
    {}

public:
    bool isSuccess() const { return mStatus == Success; }
    bool isFail() const { return mStatus == Fail; }
    bool isUnknown() const { return mStatus == Unknown; }

    Status getStatus() const { return mStatus; }

    static std::unique_ptr<SafetyResult> CreateSuccess();

    static std::unique_ptr<SafetyResult> CreateFail(unsigned ec, std::unique_ptr<Trace> trace = nullptr);

    static std::unique_ptr<SafetyResult> CreateUnknown();

    virtual ~SafetyResult() = default;

private:
    Status mStatus;
};

class FailResult final : public SafetyResult
{
public:
    FailResult(
        unsigned errorCode,
        std::unique_ptr<Trace> trace = nullptr
    ) : SafetyResult(SafetyResult::Fail), mErrorID(errorCode),
        mTrace(std::move(trace))
    {}

    Trace& getTrace() const { return *mTrace; }
    unsigned getErrorID() const { return mErrorID; }

    static bool classof(const SafetyResult* result) {
        return result->getStatus() == SafetyResult::Fail;
    }

private:
    unsigned mErrorID;
    std::unique_ptr<Trace> mTrace;
};

class SuccessResult final : public SafetyResult
{
public:
    SuccessResult()
        : SafetyResult(SafetyResult::Success)
    {}
};

class UnknownResult final : public SafetyResult
{
public:
    UnknownResult()
        : SafetyResult(SafetyResult::Unknown)
    {}
};

}

#endif