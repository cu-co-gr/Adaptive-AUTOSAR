#include "./nrc_exception.h"
#include <cstdint>

namespace ara
{
    namespace diag
    {
        namespace routing
        {
            NrcExecption::NrcExecption(uint8_t nrc) noexcept : mNrc{nrc}
            {
            }

            uint8_t NrcExecption::GetNrc() const noexcept
            {
                return mNrc;
            }
        }
    }
}