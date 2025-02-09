#include "FileOperationHandler.h"

namespace opendnp3
{
    bool FileOperationHandler::IsAllowed(uint32_t /*headerCount*/, GroupVariation gv, QualifierCode qc) {
        return (gv == GroupVariation::Group70Var3 ||
                gv == GroupVariation::Group70Var4 || 
                gv == GroupVariation::Group70Var5 ||
                gv == GroupVariation::Group70Var6 ||
                gv == GroupVariation::Group70Var7) &&
                qc == QualifierCode::FREE_FORMAT;
    }

    Group70Var3 FileOperationHandler::GetFileCommandObject() const
    {
        return fileCommandObject;
    }

    Group70Var4 FileOperationHandler::GetFileStatusObject() const
    {
        return fileStatusObject;
    }

    Group70Var5 FileOperationHandler::GetFileTransferObject() const
    {
        return fileTransferObject;
    }

    Group70Var6 FileOperationHandler::GetFileTransferStatusObject() const
    {
        return fileTransferStatusObject;
    }

    Group70Var7 FileOperationHandler::GetFileDescriptorObject() const
    {
        return fileDescriptorObject;
    }

    IINField FileOperationHandler::ProcessHeader(const FreeFormatHeader& header, const ICollection<Group70Var3>& values)
    {
        values.ReadOnlyValue(fileCommandObject);
        return IINField::Empty();
    }

    IINField FileOperationHandler::ProcessHeader(const FreeFormatHeader& /*header*/, const ICollection<Group70Var4>& values)
    {
        values.ReadOnlyValue(fileStatusObject);
        return IINField::Empty();
    }

    IINField FileOperationHandler::ProcessHeader(const FreeFormatHeader& /*header*/, const ICollection<Group70Var5>& values)
    {
        values.ReadOnlyValue(fileTransferObject);
        return IINField::Empty();
    }

    IINField FileOperationHandler::ProcessHeader(const FreeFormatHeader& /*header*/, const ICollection<Group70Var6>& values)
    {
        values.ReadOnlyValue(fileTransferStatusObject);
        return IINField::Empty();
    }

    IINField FileOperationHandler::ProcessHeader(const FreeFormatHeader& /*header*/, const ICollection<Group70Var7>& values)
    {
        values.ReadOnlyValue(fileDescriptorObject);
        return IINField::Empty();
    }
}
