// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/assert.h"
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/expected.h"

// All the constants in this file come from http://switchbrew.org/index.php?title=Error_codes

/**
 * Identifies the module which caused the error. Error codes can be propagated through a call
 * chain, meaning that this doesn't always correspond to the module where the API call made is
 * contained.
 */
enum class ErrorModule : u32 {
    Common = 0,
    Kernel = 1,
    FS = 2,
    OS = 3, // used for Memory, Thread, Mutex, Nvidia
    HTCS = 4,
    NCM = 5,
    DD = 6,
    LR = 8,
    Loader = 9,
    CMIF = 10,
    HIPC = 11,
    PM = 15,
    NS = 16,
    HTC = 18,
    NCMContent = 20,
    SM = 21,
    RO = 22,
    SDMMC = 24,
    OVLN = 25,
    SPL = 26,
    ETHC = 100,
    I2C = 101,
    GPIO = 102,
    UART = 103,
    Settings = 105,
    WLAN = 107,
    XCD = 108,
    NIFM = 110,
    Hwopus = 111,
    Bluetooth = 113,
    VI = 114,
    NFP = 115,
    Time = 116,
    FGM = 117,
    OE = 118,
    PCIe = 120,
    Friends = 121,
    BCAT = 122,
    SSLSrv = 123,
    Account = 124,
    News = 125,
    Mii = 126,
    NFC = 127,
    AM = 128,
    PlayReport = 129,
    AHID = 130,
    Qlaunch = 132,
    PCV = 133,
    OMM = 134,
    BPC = 135,
    PSM = 136,
    NIM = 137,
    PSC = 138,
    TC = 139,
    USB = 140,
    NSD = 141,
    PCTL = 142,
    BTM = 143,
    ETicket = 145,
    NGC = 146,
    ERPT = 147,
    APM = 148,
    Profiler = 150,
    ErrorUpload = 151,
    Audio = 153,
    NPNS = 154,
    NPNSHTTPSTREAM = 155,
    ARP = 157,
    SWKBD = 158,
    BOOT = 159,
    NFCMifare = 161,
    UserlandAssert = 162,
    Fatal = 163,
    NIMShop = 164,
    SPSM = 165,
    BGTC = 167,
    UserlandCrash = 168,
    SREPO = 180,
    Dauth = 181,
    HID = 202,
    LDN = 203,
    Irsensor = 205,
    Capture = 206,
    Manu = 208,
    ATK = 209,
    GRC = 212,
    Migration = 216,
    MigrationLdcServ = 217,
    GeneralWebApplet = 800,
    WifiWebAuthApplet = 809,
    WhitelistedApplet = 810,
    ShopN = 811,
};

/// Encapsulates a Horizon OS error code, allowing it to be separated into its constituent fields.
union Result {
    u32 raw;

    BitField<0, 9, ErrorModule> module;
    BitField<9, 13, u32> description;

    Result() = default;
    constexpr explicit Result(u32 raw_) : raw(raw_) {}

    constexpr Result(ErrorModule module_, u32 description_)
        : raw(module.FormatValue(module_) | description.FormatValue(description_)) {}

    [[nodiscard]] constexpr bool IsSuccess() const {
        return raw == 0;
    }

    [[nodiscard]] constexpr bool IsError() const {
        return !IsSuccess();
    }

    [[nodiscard]] constexpr bool IsFailure() const {
        return !IsSuccess();
    }

    [[nodiscard]] constexpr u32 GetInnerValue() const {
        return static_cast<u32>(module.Value()) | (description << module.bits);
    }

    [[nodiscard]] constexpr bool Includes(Result result) const {
        return GetInnerValue() == result.GetInnerValue();
    }
};
static_assert(std::is_trivial_v<Result>);

[[nodiscard]] constexpr bool operator==(const Result& a, const Result& b) {
    return a.raw == b.raw;
}

[[nodiscard]] constexpr bool operator!=(const Result& a, const Result& b) {
    return !operator==(a, b);
}

// Convenience functions for creating some common kinds of errors:

/// The default success `Result`.
constexpr Result ResultSuccess(0);

/**
 * Placeholder result code used for unknown error codes.
 *
 * @note This should only be used when a particular error code
 *       is not known yet.
 */
constexpr Result ResultUnknown(UINT32_MAX);

/**
 * A ResultRange defines an inclusive range of error descriptions within an error module.
 * This can be used to check whether the description of a given Result falls within the range.
 * The conversion function returns a Result with its description set to description_start.
 *
 * An example of how it could be used:
 * \code
 * constexpr ResultRange ResultCommonError{ErrorModule::Common, 0, 9999};
 *
 * Result Example(int value) {
 *     const Result result = OtherExample(value);
 *
 *     // This will only evaluate to true if result.module is ErrorModule::Common and
 *     // result.description is in between 0 and 9999 inclusive.
 *     if (ResultCommonError.Includes(result)) {
 *         // This returns Result{ErrorModule::Common, 0};
 *         return ResultCommonError;
 *     }
 *
 *     return ResultSuccess;
 * }
 * \endcode
 */
class ResultRange {
public:
    consteval ResultRange(ErrorModule module, u32 description_start, u32 description_end_)
        : code{module, description_start}, description_end{description_end_} {}

    [[nodiscard]] constexpr operator Result() const {
        return code;
    }

    [[nodiscard]] constexpr bool Includes(Result other) const {
        return code.module == other.module && code.description <= other.description &&
               other.description <= description_end;
    }

private:
    Result code;
    u32 description_end;
};

/**
 * This is an optional value type. It holds a `Result` and, if that code is ResultSuccess, it
 * also holds a result of type `T`. If the code is an error code (not ResultSuccess), then trying
 * to access the inner value with operator* is undefined behavior and will assert with Unwrap().
 * Users of this class must be cognizant to check the status of the ResultVal with operator bool(),
 * Code(), Succeeded() or Failed() prior to accessing the inner value.
 *
 * An example of how it could be used:
 * \code
 * ResultVal<int> Frobnicate(float strength) {
 *     if (strength < 0.f || strength > 1.0f) {
 *         // Can't frobnicate too weakly or too strongly
 *         return Result{ErrorModule::Common, 1};
 *     } else {
 *         // Frobnicated! Give caller a cookie
 *         return 42;
 *     }
 * }
 * \endcode
 *
 * \code
 * auto frob_result = Frobnicate(0.75f);
 * if (frob_result) {
 *     // Frobbed ok
 *     printf("My cookie is %d\n", *frob_result);
 * } else {
 *     printf("Guess I overdid it. :( Error code: %ux\n", frob_result.Code().raw);
 * }
 * \endcode
 */
template <typename T>
class ResultVal {
public:
    constexpr ResultVal() : expected{} {}

    constexpr ResultVal(Result code) : expected{Common::Unexpected(code)} {}

    constexpr ResultVal(ResultRange range) : expected{Common::Unexpected(range)} {}

    template <typename U>
    constexpr ResultVal(U&& val) : expected{std::forward<U>(val)} {}

    template <typename... Args>
    constexpr ResultVal(Args&&... args) : expected{std::in_place, std::forward<Args>(args)...} {}

    ~ResultVal() = default;

    constexpr ResultVal(const ResultVal&) = default;
    constexpr ResultVal(ResultVal&&) = default;

    ResultVal& operator=(const ResultVal&) = default;
    ResultVal& operator=(ResultVal&&) = default;

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return expected.has_value();
    }

    [[nodiscard]] constexpr Result Code() const {
        return expected.has_value() ? ResultSuccess : expected.error();
    }

    [[nodiscard]] constexpr bool Succeeded() const {
        return expected.has_value();
    }

    [[nodiscard]] constexpr bool Failed() const {
        return !expected.has_value();
    }

    [[nodiscard]] constexpr T* operator->() {
        return std::addressof(expected.value());
    }

    [[nodiscard]] constexpr const T* operator->() const {
        return std::addressof(expected.value());
    }

    [[nodiscard]] constexpr T& operator*() & {
        return *expected;
    }

    [[nodiscard]] constexpr const T& operator*() const& {
        return *expected;
    }

    [[nodiscard]] constexpr T&& operator*() && {
        return *expected;
    }

    [[nodiscard]] constexpr const T&& operator*() const&& {
        return *expected;
    }

    [[nodiscard]] constexpr T& Unwrap() & {
        ASSERT_MSG(Succeeded(), "Tried to Unwrap empty ResultVal");
        return expected.value();
    }

    [[nodiscard]] constexpr const T& Unwrap() const& {
        ASSERT_MSG(Succeeded(), "Tried to Unwrap empty ResultVal");
        return expected.value();
    }

    [[nodiscard]] constexpr T&& Unwrap() && {
        ASSERT_MSG(Succeeded(), "Tried to Unwrap empty ResultVal");
        return std::move(expected.value());
    }

    [[nodiscard]] constexpr const T&& Unwrap() const&& {
        ASSERT_MSG(Succeeded(), "Tried to Unwrap empty ResultVal");
        return std::move(expected.value());
    }

    template <typename U>
    [[nodiscard]] constexpr T ValueOr(U&& v) const& {
        return expected.value_or(v);
    }

    template <typename U>
    [[nodiscard]] constexpr T ValueOr(U&& v) && {
        return expected.value_or(v);
    }

private:
    // TODO (Morph): Replace this with C++23 std::expected.
    Common::Expected<T, Result> expected;
};

/**
 * Check for the success of `source` (which must evaluate to a ResultVal). If it succeeds, unwraps
 * the contained value and assigns it to `target`, which can be either an l-value expression or a
 * variable declaration. If it fails the return code is returned from the current function. Thus it
 * can be used to cascade errors out, achieving something akin to exception handling.
 */
#define CASCADE_RESULT(target, source)                                                             \
    auto CONCAT2(check_result_L, __LINE__) = source;                                               \
    if (CONCAT2(check_result_L, __LINE__).Failed()) {                                              \
        return CONCAT2(check_result_L, __LINE__).Code();                                           \
    }                                                                                              \
    target = std::move(*CONCAT2(check_result_L, __LINE__))

/**
 * Analogous to CASCADE_RESULT, but for a bare Result. The code will be propagated if
 * non-success, or discarded otherwise.
 */
#define CASCADE_CODE(source)                                                                       \
    do {                                                                                           \
        auto CONCAT2(check_result_L, __LINE__) = source;                                           \
        if (CONCAT2(check_result_L, __LINE__).IsError()) {                                         \
            return CONCAT2(check_result_L, __LINE__);                                              \
        }                                                                                          \
    } while (false)

#define R_SUCCEEDED(res) (static_cast<Result>(res).IsSuccess())
#define R_FAILED(res) (static_cast<Result>(res).IsFailure())

namespace ResultImpl {
template <auto EvaluateResult, class F>
class ScopedResultGuard {
    YUZU_NON_COPYABLE(ScopedResultGuard);
    YUZU_NON_MOVEABLE(ScopedResultGuard);

private:
    Result& m_ref;
    F m_f;

public:
    constexpr ScopedResultGuard(Result& ref, F f) : m_ref(ref), m_f(std::move(f)) {}
    constexpr ~ScopedResultGuard() {
        if (EvaluateResult(m_ref)) {
            m_f();
        }
    }
};

template <auto EvaluateResult>
class ResultReferenceForScopedResultGuard {
private:
    Result& m_ref;

public:
    constexpr ResultReferenceForScopedResultGuard(Result& r) : m_ref(r) {}
    constexpr operator Result&() const {
        return m_ref;
    }
};

template <auto EvaluateResult, typename F>
constexpr ScopedResultGuard<EvaluateResult, F> operator+(
    ResultReferenceForScopedResultGuard<EvaluateResult> ref, F&& f) {
    return ScopedResultGuard<EvaluateResult, F>(static_cast<Result&>(ref), std::forward<F>(f));
}

constexpr bool EvaluateResultSuccess(const Result& r) {
    return R_SUCCEEDED(r);
}
constexpr bool EvaluateResultFailure(const Result& r) {
    return R_FAILED(r);
}

template <typename T>
constexpr void UpdateCurrentResultReference(T result_reference, Result result) = delete;
// Intentionally not defined

template <>
constexpr void UpdateCurrentResultReference<Result&>(Result& result_reference, Result result) {
    result_reference = result;
}

template <>
constexpr void UpdateCurrentResultReference<const Result>(Result result_reference, Result result) {}
} // namespace ResultImpl

#define DECLARE_CURRENT_RESULT_REFERENCE_AND_STORAGE(COUNTER_VALUE)                                \
    [[maybe_unused]] constexpr bool CONCAT2(HasPrevRef_, COUNTER_VALUE) =                          \
        std::same_as<decltype(__TmpCurrentResultReference), Result&>;                              \
    [[maybe_unused]] Result CONCAT2(PrevRef_, COUNTER_VALUE) = __TmpCurrentResultReference;        \
    [[maybe_unused]] Result CONCAT2(__tmp_result_, COUNTER_VALUE) = ResultSuccess;                 \
    Result& __TmpCurrentResultReference = CONCAT2(HasPrevRef_, COUNTER_VALUE)                      \
                                              ? CONCAT2(PrevRef_, COUNTER_VALUE)                   \
                                              : CONCAT2(__tmp_result_, COUNTER_VALUE)

#define ON_RESULT_RETURN_IMPL(...)                                                                 \
    static_assert(std::same_as<decltype(__TmpCurrentResultReference), Result&>);                   \
    auto CONCAT2(RESULT_GUARD_STATE_, __COUNTER__) =                                               \
        ResultImpl::ResultReferenceForScopedResultGuard<__VA_ARGS__>(                              \
            __TmpCurrentResultReference) +                                                         \
        [&]()

#define ON_RESULT_FAILURE_2 ON_RESULT_RETURN_IMPL(ResultImpl::EvaluateResultFailure)

#define ON_RESULT_FAILURE                                                                          \
    DECLARE_CURRENT_RESULT_REFERENCE_AND_STORAGE(__COUNTER__);                                     \
    ON_RESULT_FAILURE_2

#define ON_RESULT_SUCCESS_2 ON_RESULT_RETURN_IMPL(ResultImpl::EvaluateResultSuccess)

#define ON_RESULT_SUCCESS                                                                          \
    DECLARE_CURRENT_RESULT_REFERENCE_AND_STORAGE(__COUNTER__);                                     \
    ON_RESULT_SUCCESS_2

constexpr inline Result __TmpCurrentResultReference = ResultSuccess;

/// Returns a result.
#define R_RETURN(res_expr)                                                                         \
    {                                                                                              \
        const Result _tmp_r_throw_rc = (res_expr);                                                 \
        ResultImpl::UpdateCurrentResultReference<decltype(__TmpCurrentResultReference)>(           \
            __TmpCurrentResultReference, _tmp_r_throw_rc);                                         \
        return _tmp_r_throw_rc;                                                                    \
    }

/// Returns ResultSuccess()
#define R_SUCCEED() R_RETURN(ResultSuccess)

/// Throws a result.
#define R_THROW(res_expr) R_RETURN(res_expr)

/// Evaluates a boolean expression, and returns a result unless that expression is true.
#define R_UNLESS(expr, res)                                                                        \
    {                                                                                              \
        if (!(expr)) {                                                                             \
            R_THROW(res);                                                                          \
        }                                                                                          \
    }

/// Evaluates an expression that returns a result, and returns the result if it would fail.
#define R_TRY(res_expr)                                                                            \
    {                                                                                              \
        const auto _tmp_r_try_rc = (res_expr);                                                     \
        if (R_FAILED(_tmp_r_try_rc)) {                                                             \
            R_THROW(_tmp_r_try_rc);                                                                \
        }                                                                                          \
    }

/// Evaluates a boolean expression, and succeeds if that expression is true.
#define R_SUCCEED_IF(expr) R_UNLESS(!(expr), ResultSuccess)
