#include <stdexcept>
#include "./per_error_domain.h"

namespace ara
{
    namespace per
    {
        const ara::core::ErrorDomain::IdType PerErrorDomain::cId;
        PerErrorDomain *PerErrorDomain::mInstance = nullptr;

        const char *PerErrorDomain::Name() const noexcept
        {
            return cName;
        }

        const char *PerErrorDomain::Message(
            ara::core::ErrorDomain::CodeType errorCode) const noexcept
        {
            auto _errc{static_cast<ara::per::PerErrc>(errorCode)};

            switch (_errc)
            {
            case ara::per::PerErrc::kStorageNotFound:
                return "Storage database not found";
            case ara::per::PerErrc::kKeyNotFound:
                return "Key does not exist in storage";
            case ara::per::PerErrc::kIllegalWriteAccess:
                return "Write access not permitted";
            case ara::per::PerErrc::kPhysicalStorageFailure:
                return "I/O error on backing file";
            case ara::per::PerErrc::kIntegrityCorrupted:
                return "Storage file is corrupt or unparseable";
            case ara::per::PerErrc::kOutOfStorageSpace:
                return "No space left on device";
            case ara::per::PerErrc::kInitValueNotAvailable:
                return "No default/init value configured";
            case ara::per::PerErrc::kResourceBusy:
                return "Resource is locked or in use";
            case ara::per::PerErrc::kFileNotFound:
                return "Named file does not exist in storage";
            default:
                return "Not supported";
            }
        }

        ara::core::ErrorDomain *PerErrorDomain::GetPerDomain()
        {
            if (mInstance == nullptr)
            {
                mInstance = new PerErrorDomain();
            }

            return mInstance;
        }

        void PerErrorDomain::ThrowAsException(
            const ara::core::ErrorCode &errorCode) const noexcept(false)
        {
            throw std::runtime_error{Message(errorCode.Value())};
        }

        ara::core::ErrorCode PerErrorDomain::MakeErrorCode(PerErrc code) noexcept
        {
            auto _codeType{static_cast<ara::core::ErrorDomain::CodeType>(code)};
            ara::core::ErrorCode _result(_codeType, *mInstance);

            return _result;
        }

        ara::core::ErrorCode MakeErrorCode(PerErrc code) noexcept
        {
            auto *_domain =
                static_cast<PerErrorDomain *>(PerErrorDomain::GetPerDomain());
            return _domain->MakeErrorCode(code);
        }
    }
}
