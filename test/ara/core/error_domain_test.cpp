#include <stdexcept>
#include <gtest/gtest.h>
#include "../../../src/ara/core/error_domain.h"
#include "../../../src/ara/core/error_code.h"

namespace ara
{
    namespace core
    {
        class ErrorDomainTest : public testing::Test, public ErrorDomain
        {
        protected:
            static const IdType cTestDomainId = 1;

        public:
            ErrorDomainTest() : ErrorDomain{cTestDomainId}
            {
            }

            virtual const char *Name() const noexcept override
            {
                return "";
            }

            virtual const char *Message(CodeType errorCode) const noexcept override
            {
                return "";
            }

            virtual void ThrowAsException(const ErrorCode &errorCode) const noexcept(false) override
            {
                throw std::runtime_error{Message(errorCode.Value())};
            }
        };

        const ErrorDomain::IdType ErrorDomainTest::cTestDomainId;

        TEST_F(ErrorDomainTest, Constructor)
        {
            EXPECT_EQ(cTestDomainId, Id());
        }
    }
}