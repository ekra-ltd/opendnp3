#include "FileOperationHandler.h"

namespace opendnp3
{
    bool FileOperationHandler::IsAllowed(uint32_t /*headerCount*/, GroupVariation gv, QualifierCode qc) {
        return (gv == GroupVariation::Group70Var4 || gv == GroupVariation::Group70Var5) && qc == QualifierCode::FREE_FORMAT;
    }

    Group70Var4 FileOperationHandler::GetFileStatusObject() const
    {
        return fileStatusObject;
    }

    Group70Var5 FileOperationHandler::GetFileTransferObject() const
    {
        return fileTransferObject;
    }

    IINField FileOperationHandler::ProcessHeader(const FreeFormatHeader& header, const ICollection<Group70Var4>& values)
    {
        values.ReadOnlyValue(fileStatusObject);
        return IINField::Empty();
    }

    IINField FileOperationHandler::ProcessHeader(const FreeFormatHeader& header, const ICollection<Group70Var5>& values)
    {
        values.ReadOnlyValue(fileTransferObject);
        return IINField::Empty();
    }
}
