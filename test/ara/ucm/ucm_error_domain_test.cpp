#include <gtest/gtest.h>
#include "../../../src/ara/ucm/ucm_error_domain.h"

namespace ara
{
    namespace ucm
    {
        TEST(UcmErrorDomainTest, NameProperty)
        {
            ara::core::ErrorDomain *_domain{UcmErrorDomain::GetUcmDomain()};
            EXPECT_STREQ(_domain->Name(), "Ucm");
        }

        TEST(UcmErrorDomainTest, MakeErrorCodeMethod)
        {
            const UcmErrc cExpected{UcmErrc::kInvalidTransferId};
            auto *_domain{
                static_cast<UcmErrorDomain *>(UcmErrorDomain::GetUcmDomain())};
            ara::core::ErrorCode _code{_domain->MakeErrorCode(cExpected)};
            auto _actual{static_cast<UcmErrc>(_code.Value())};
            EXPECT_EQ(cExpected, _actual);
        }

        TEST(UcmErrorDomainTest, FreeMakeErrorCode)
        {
            ara::core::ErrorCode _code{
                MakeErrorCode(UcmErrc::kOperationInProgress)};
            auto _actual{static_cast<UcmErrc>(_code.Value())};
            EXPECT_EQ(UcmErrc::kOperationInProgress, _actual);
        }

        TEST(UcmErrorDomainTest, AllCodesHaveMessages)
        {
            auto *_domain{UcmErrorDomain::GetUcmDomain()};
            const UcmErrc cCodes[] = {
                UcmErrc::kInvalidTransferId,
                UcmErrc::kTransferNotComplete,
                UcmErrc::kPackageMalformed,
                UcmErrc::kActivationFailed,
                UcmErrc::kRollbackFailed,
                UcmErrc::kInsufficientSpace,
                UcmErrc::kOperationInProgress
            };
            for (auto _errc : cCodes)
            {
                auto _code{static_cast<ara::core::ErrorDomain::CodeType>(_errc)};
                const char *_msg{_domain->Message(_code)};
                EXPECT_STRNE(_msg, "Not supported") << "errc=" << _code;
            }
        }

        TEST(UcmErrorDomainTest, ThrowAsException)
        {
            auto *_domain{UcmErrorDomain::GetUcmDomain()};
            auto _code{MakeErrorCode(UcmErrc::kPackageMalformed)};
            EXPECT_THROW(_domain->ThrowAsException(_code), std::runtime_error);
        }

    } // namespace ucm
} // namespace ara
