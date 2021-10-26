#pragma once
#include "app/parsing/IAPDUHandler.h"
#include "gen/objects/Group70.h"

namespace opendnp3
{
    class FileOperationHandler : public opendnp3::IAPDUHandler
    {
    public:
        bool IsAllowed(uint32_t headerCount, GroupVariation gv, QualifierCode qc) override;
        Group70Var4 GetFileStatusObject() const;
        Group70Var5 GetFileTransferObject() const;
        Group70Var6 GetFileTransferStatusObject() const;

    protected:
        IINField ProcessHeader(const FreeFormatHeader& header, const ICollection<Group70Var4>& values) override;
        IINField ProcessHeader(const FreeFormatHeader& header, const ICollection<Group70Var5>& values) override;
        IINField ProcessHeader(const FreeFormatHeader& header, const ICollection<Group70Var6>& values) override;

    private:
        Group70Var4 fileStatusObject;
        Group70Var5 fileTransferObject;
        Group70Var6 fileTransferStatusObject;
    };
}
