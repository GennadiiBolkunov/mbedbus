#include "mbedbus/Message.h"

// Message is essentially header-only. This translation unit ensures the library
// links correctly even if no out-of-line definitions are needed today.

namespace mbedbus {
// Intentionally empty — all Message methods are inline in the header.
} // namespace mbedbus
