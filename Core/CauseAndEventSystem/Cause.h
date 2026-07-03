#pragma once
#include "CauseGUID.h"
#include <memory>

namespace CauseAndEventSystem
{

class Cause
{
public:
    Cause() = default;
    Cause(const Cause&) = delete;
    Cause& operator=(const Cause&) = delete;
    Cause(Cause&&) = default;
    Cause& operator=(Cause&&) = default;

    virtual ~Cause() = default;

    virtual bool CheckCondition(double deltaTime) = 0;
    virtual void Reset() = 0;
    virtual const char* GetCauseName() const = 0;

    void SetOwnerGUID(const CauseGUID& guid) { m_ownerGUID = guid; }
    const CauseGUID& GetOwnerGUID() const { return m_ownerGUID; }

private:
    CauseGUID m_ownerGUID;
};

}
