#pragma once

#include <cassert>
#include <cstdlib>
#include <limits>
#include <type_traits>

// Assumes phx::equity::shares looks roughly like:
//
//   namespace phx::equity {
//     struct shares {
//       using scalar_type = ...;           // integral
//       scalar_type value() const noexcept;
//       explicit shares(scalar_type);
//     };
//   }

class fractional_shares {
public:
    using shares_type = phx::equity::shares;
    using raw_value_t = typename shares_type::scalar_type;

    static_assert(std::is_integral<raw_value_t>::value,
                  "shares::scalar_type must be an integral type");

    static constexpr raw_value_t SIP_SHARE_MULTIPLIER = 1'000'000; // micro-shares

    // Tag type to make "raw" construction explicit at the call site.
    struct from_raw_value_t {
        explicit constexpr from_raw_value_t() = default;
    };
    static constexpr from_raw_value_t from_raw_value{};

    constexpr fractional_shares() noexcept = default;

    // Whole shares -> fixed-point micro-shares.
    explicit constexpr fractional_shares(shares_type s) noexcept
        : raw_value_(checked_mul_by_multiplier_(s.value())) {}

    // Raw micro-shares.
    constexpr fractional_shares(from_raw_value_t, raw_value_t raw) noexcept
        : raw_value_(raw) {}

    [[nodiscard]] constexpr raw_value_t raw_value() const noexcept { return raw_value_; }

    // Approximate, human-friendly numeric form.
    [[nodiscard]] constexpr double value() const noexcept {
        return static_cast<double>(raw_value_) / static_cast<double>(SIP_SHARE_MULTIPLIER);
    }

    // Truncates toward zero (C++ integer division semantics).
    [[nodiscard]] constexpr shares_type whole_shares_trunc() const noexcept {
        return shares_type{raw_value_ / SIP_SHARE_MULTIPLIER};
    }

    constexpr explicit operator shares_type() const noexcept { return whole_shares_trunc(); }

    // --- arithmetic ---

    constexpr fractional_shares& operator+=(fractional_shares rhs) noexcept {
        raw_value_ = checked_add_(raw_value_, rhs.raw_value_);
        return *this;
    }

    constexpr fractional_shares& operator-=(fractional_shares rhs) noexcept {
        raw_value_ = checked_sub_(raw_value_, rhs.raw_value_);
        return *this;
    }

    friend constexpr fractional_shares operator+(fractional_shares lhs,
                                                  fractional_shares rhs) noexcept {
        lhs += rhs;
        return lhs;
    }

    friend constexpr fractional_shares operator-(fractional_shares lhs,
                                                  fractional_shares rhs) noexcept {
        lhs -= rhs;
        return lhs;
    }

    // --- comparisons ---

    friend constexpr bool operator==(fractional_shares a, fractional_shares b) noexcept {
        return a.raw_value_ == b.raw_value_;
    }
    friend constexpr bool operator!=(fractional_shares a, fractional_shares b) noexcept {
        return !(a == b);
    }

    friend constexpr bool operator<(fractional_shares a, fractional_shares b) noexcept {
        return a.raw_value_ < b.raw_value_;
    }
    friend constexpr bool operator<=(fractional_shares a, fractional_shares b) noexcept {
        return !(b < a);
    }
    friend constexpr bool operator>(fractional_shares a, fractional_shares b) noexcept {
        return b < a;
    }
    friend constexpr bool operator>=(fractional_shares a, fractional_shares b) noexcept {
        return !(a < b);
    }

private:
    raw_value_t raw_value_{0};

    [[noreturn]] static void overflow_() noexcept {
        assert(false && "fractional_shares overflow");
        std::abort(); // keeps you safe even when asserts are compiled out
    }

    static constexpr raw_value_t checked_mul_by_multiplier_(raw_value_t whole) noexcept {
        static_assert(SIP_SHARE_MULTIPLIER > 0, "multiplier must be positive");

        const raw_value_t max = std::numeric_limits<raw_value_t>::max();

        // Prevent UB: check *before* multiplying.
        if (whole > max / SIP_SHARE_MULTIPLIER) overflow_();
        if constexpr (std::numeric_limits<raw_value_t>::is_signed) {
            const raw_value_t min = std::numeric_limits<raw_value_t>::min();
            if (whole < min / SIP_SHARE_MULTIPLIER) overflow_();
        }
        return whole * SIP_SHARE_MULTIPLIER;
    }

    static constexpr raw_value_t checked_add_(raw_value_t a, raw_value_t b) noexcept {
        const raw_value_t max = std::numeric_limits<raw_value_t>::max();

        // Prevent UB: check *before* adding.
        if (b > 0 && a > max - b) overflow_();
        if constexpr (std::numeric_limits<raw_value_t>::is_signed) {
            const raw_value_t min = std::numeric_limits<raw_value_t>::min();
            if (b < 0 && a < min - b) overflow_();
        }
        return a + b;
    }

    static constexpr raw_value_t checked_sub_(raw_value_t a, raw_value_t b) noexcept {
        const raw_value_t max = std::numeric_limits<raw_value_t>::max();

        // Prevent UB: check *before* subtracting.
        if constexpr (std::numeric_limits<raw_value_t>::is_signed) {
            const raw_value_t min = std::numeric_limits<raw_value_t>::min();
            if (b > 0 && a < min + b) overflow_();
            if (b < 0 && a > max + b) overflow_();
        } else {
            if (a < b) overflow_();
        }
        return a - b;
    }
};
