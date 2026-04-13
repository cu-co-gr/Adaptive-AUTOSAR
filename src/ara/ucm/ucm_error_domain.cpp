#include <stdexcept>
#include "./ucm_error_domain.h"

namespace ara
{
    namespace ucm
    {
        const ara::core::ErrorDomain::IdType UcmErrorDomain::cId;
        UcmErrorDomain *UcmErrorDomain::mInstance = nullptr;

        const char *UcmErrorDomain::Name() const noexcept
        {
            return cName;
        }

        const char *UcmErrorDomain::Message(
            ara::core::ErrorDomain::CodeType errorCode) const noexcept
        {
            auto _errc{static_cast<ara::ucm::UcmErrc>(errorCode)};

            switch (_errc)
            {
            case ara::ucm::UcmErrc::kInvalidTransferId:
                return "Transfer ID is unknown or already finalised";
            case ara::ucm::UcmErrc::kTransferNotComplete:
                return "TransferExit called before all data received";
            case ara::ucm::UcmErrc::kPackageMalformed:
                return "SW Package cannot be parsed or extracted";
            case ara::ucm::UcmErrc::kActivationFailed:
                return "Activation step failed (binary copy or manifest)";
            case ara::ucm::UcmErrc::kRollbackFailed:
                return "Rollback could not restore previous state";
            case ara::ucm::UcmErrc::kInsufficientSpace:
                return "Not enough disk space for the SW Package";
            case ara::ucm::UcmErrc::kOperationInProgress:
                return "UCM is already processing another transfer";
            default:
                return "Not supported";
            }
        }

        ara::core::ErrorDomain *UcmErrorDomain::GetUcmDomain()
        {
            if (mInstance == nullptr)
            {
                mInstance = new UcmErrorDomain();
            }

            return mInstance;
        }

        void UcmErrorDomain::ThrowAsException(
            const ara::core::ErrorCode &errorCode) const noexcept(false)
        {
            throw std::runtime_error{Message(errorCode.Value())};
        }

        ara::core::ErrorCode UcmErrorDomain::MakeErrorCode(UcmErrc code) noexcept
        {
            auto _codeType{static_cast<ara::core::ErrorDomain::CodeType>(code)};
            ara::core::ErrorCode _result(_codeType, *mInstance);
            return _result;
        }

        ara::core::ErrorCode MakeErrorCode(UcmErrc code) noexcept
        {
            auto *_domain{
                static_cast<UcmErrorDomain *>(UcmErrorDomain::GetUcmDomain())};
            return _domain->MakeErrorCode(code);
        }

    } // namespace ucm
} // namespace ara
