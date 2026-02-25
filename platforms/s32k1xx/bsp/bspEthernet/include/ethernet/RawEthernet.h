// Copyright 2025 Accenture.

#pragma once

#ifdef __cplusplus

#include "etl/chrono.h"
#include "etl/delegate.h"
#include "etl/span.h"

namespace ethernet
{

typedef ::etl::delegate<void(
    ::etl::span<uint8_t const>, // data
    ::etl::chrono::nanoseconds  // systemTicks
    )>
    DataSentCallback;

} // namespace ethernet

#endif // #ifdef __cplusplus
