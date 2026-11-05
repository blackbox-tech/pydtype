//----------------------------------------------------------------------------------------------------------------------
// C++26 reflection to numpy dtypes
//
// For this code to compile with gcc16 include the following flags: -std=c++26 -freflection
//
// Author: Paul Scruby
// Copyright: Paul Scruby
// License: MIT
//

#pragma once

#include <bit>
#include <chrono>
#include <concepts>
#include <meta>
#include <string>
#include <string_view>
#include <type_traits>

namespace pydtype {

enum class Endian {
    NONE,
    LITTLE,
    BIG,
    NATIVE,
};

namespace detail {

template <std::size_t N>
struct FixedString {
    char data[N]{};

    // Allows implicit conversion to a std::string_view
    constexpr operator std::string_view() const {
        return std::string_view(data, N);
    }
};

template <std::size_t N>
consteval auto const_to_string() {
    if constexpr (N == 0) {
        return FixedString<1>{{'0'}};
    } else {
        constexpr auto count_digits = [](std::size_t val) {
            std::size_t len = 0;

            while (val > 0) {
                len++;
                val /= 10;
            }

            return len;
        };

        constexpr std::size_t len = count_digits(N);
        FixedString<len> buffer;
        std::size_t tmp = N;

        for (std::size_t i = 0; i < len; i++) {
            buffer.data[len - 1 - i] = static_cast<char>('0' + (tmp % 10));
            tmp /= 10;
        }

        return buffer;
    }
}

template <typename T>
consteval std::string_view array_dims() {
    if constexpr (std::is_array_v<T>) {
        static constexpr std::size_t ext = std::extent_v<T>;
        static constexpr auto num_str = const_to_string<ext>();
        static constexpr auto next_view = array_dims<std::remove_extent_t<T>>();

        constexpr std::size_t total_size = 2 + sizeof(num_str.data) + next_view.size();

        // Statically cache the block so the std::string_view points to a valid lifetime
        static constexpr auto assembled = []() {
            FixedString<total_size> buffer;
            buffer.data[0] = ',';
            buffer.data[1] = ' ';
            std::size_t idx = 2;

            for (std::size_t i = 0; i < sizeof(num_str.data); i++) {
                buffer.data[idx++] = num_str.data[i];
            }

            for (std::size_t i = 0; i < next_view.size(); i++) {
                buffer.data[idx++] = next_view[i];
            }
            return buffer;
        }();

        return std::string_view(assembled.data, sizeof(assembled.data));
    } else {
        return "";
    }
}

template<typename T> struct always_false : std::false_type {};
template<typename T> struct is_chrono_time_point : std::false_type {};
template<typename C, typename D> struct is_chrono_time_point<std::chrono::time_point<C, D>> : std::true_type {};
template<typename T> struct is_chrono_duration : std::false_type {};
template<typename R, typename P> struct is_chrono_duration<std::chrono::duration<R, P>> : std::true_type {};
template<typename T> struct underlying_type { using type = T; };
template<typename T> requires std::is_enum_v<T> struct underlying_type<T> { using type = std::underlying_type_t<T>; };
template<typename T> using underlying_type_t = typename underlying_type<T>::type;

template<Endian E> consteval std::string_view endian_prefix();
template<> consteval std::string_view endian_prefix<Endian::NONE>() { return ""; }
template<> consteval std::string_view endian_prefix<Endian::LITTLE>() { return "<"; }
template<> consteval std::string_view endian_prefix<Endian::BIG>() { return ">"; }
template<> consteval std::string_view endian_prefix<Endian::NATIVE>() { return std::endian::native == std::endian::little ? "<" : ">"; }

template<typename T>
consteval std::string_view np_scalar_type() {
    if constexpr (is_chrono_time_point<T>::value) {
        using duration_t = typename T::duration;

        if constexpr (std::same_as<duration_t, std::chrono::nanoseconds>) {
            return "M8[ns]";
        } else if constexpr (std::same_as<duration_t, std::chrono::microseconds>) {
            return "M8[us]";
        } else if constexpr (std::same_as<duration_t, std::chrono::milliseconds>) {
            return "M8[ms]";
        } else if constexpr (std::same_as<duration_t, std::chrono::seconds>) {
            return "M8[s]";
        } else if constexpr (std::same_as<duration_t, std::chrono::minutes>) {
            return "M8[m]";
        } else if constexpr (std::same_as<duration_t, std::chrono::hours>) {
            return "M8[h]";
        } else {
            static_assert(always_false<T>::value, "Unsupported std::chrono::time_point resolution for dtype description");
        }
    } else if constexpr (is_chrono_duration<T>::value) {
        if constexpr (std::same_as<T, std::chrono::nanoseconds>) {
            return "m8[ns]";
        } else if constexpr (std::same_as<T, std::chrono::microseconds>) {
            return "m8[us]";
        } else if constexpr (std::same_as<T, std::chrono::milliseconds>) {
            return "m8[ms]";
        } else if constexpr (std::same_as<T, std::chrono::seconds>) {
            return "m8[s]";
        } else if constexpr (std::same_as<T, std::chrono::minutes>) {
            return "m8[m]";
        } else if constexpr (std::same_as<T, std::chrono::hours>) {
            return "m8[h]";
        } else {
            static_assert(always_false<T>::value, "Unsupported std::chrono::duration resolution for dtype description");
        }
    } else if constexpr (std::same_as<T, bool>) {
        return "b1";
    } else if constexpr (std::same_as<T, char>) {
        return "S1";
    } else if constexpr ((std::same_as<T, signed char>) or (std::same_as<T, int8_t>)) {
       return "i1";
    } else if constexpr ((std::same_as<T, unsigned char>) or (std::same_as<T, uint8_t>) or (std::same_as<T, std::byte>)) {
        return "u1";
    } else if constexpr ((std::same_as<T, short>) or (std::same_as<T, int16_t>)) {
        return "i2";
    } else if constexpr ((std::same_as<T, unsigned short>) or (std::same_as<T, uint16_t>)) {
        return "u2";
    } else if constexpr (std::same_as<T, char16_t>) {
        return "U2";
    } else if constexpr (std::same_as<T, int32_t>) {
        return "i4";
    } else if constexpr (std::same_as<T, uint32_t>) {
        return "u4";
    } else if constexpr (std::same_as<T, char32_t>) {
        return "U4";
    } else if constexpr (std::same_as<T, long long> or std::same_as<T, int64_t>) {
        return "i8";
    } else if constexpr (std::same_as<T, unsigned long long> or std::same_as<T, uint64_t>) {
        return "u8";
    } else if constexpr (std::same_as<T, float>) {
        return "f4";
    } else if constexpr (std::same_as<T, double>) {
        return "f8";
    } else if constexpr (std::same_as<T, long double>) {
        if constexpr (sizeof(long double) == 8) {
            return "f8";
        } else if constexpr (sizeof(long double) == 12) {
            return "f12";
        } else if constexpr (sizeof(long double) == 16) {
            return "f16";
        } else {
            return "g";
        }
    }

    return "";
}

template<typename T>
consteval bool explicit_layout() {
    using UncvR = underlying_type_t<std::remove_cvref_t<T>>;

    if constexpr (not (std::is_class_v<UncvR> or std::is_union_v<UncvR>)) {
        return false;
    }

    constexpr auto ctx = std::meta::access_context::unchecked();
    static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(^^UncvR, ctx));
    std::size_t expected_offset = 0;
    bool is_explicit = false;

    template for (constexpr std::meta::info member : members) {
        using FieldType = typename[:std::meta::type_of(member):];
        constexpr std::string_view field_name = std::meta::identifier_of(member);

        if constexpr (std::is_union_v<FieldType> and field_name.empty()) {
            is_explicit = true;
        } else {
            constexpr std::size_t actual_offset = std::is_union_v<UncvR> ? 0 : std::meta::offset_of(member).bytes;
            if (actual_offset != expected_offset) {
                is_explicit = true;
            }
            expected_offset = actual_offset + sizeof(FieldType);
        }
    }

    if (sizeof(UncvR) != expected_offset) {
        is_explicit = true;
    }

    return is_explicit;
}

} // namespace detail

//----------------------------------------------------------------------------------------------------------------------
// This function generates a numpy dtype description for a C structure using reflection. It supports nested types,
// multi-dimensional arrays, enums, named and anonymous unions, and chrono to datetime64 and timedelta64 conversion.
// Where the numpy default alignment differs from C it will update the dtype to match.
//
template <typename T, Endian E = Endian::NONE>
std::string desc() {
    using RawT = std::remove_cvref_t<T>;
    using UncvR = detail::underlying_type_t<RawT>;

    if constexpr (std::is_array_v<UncvR>) {
        using ElementType = std::remove_all_extents_t<UncvR>;

        if constexpr (std::same_as<ElementType, char>) {
            return "'S" + std::to_string(sizeof(UncvR)) + "'";
        } else if constexpr (std::same_as<ElementType, char16_t>) {
            return "'" + std::string(detail::endian_prefix<E>()) + "U" + std::to_string(sizeof(UncvR) / 2) + "'";
        } else if constexpr (std::same_as<ElementType, char32_t>) {
            return "'" + std::string(detail::endian_prefix<E>()) + "U" + std::to_string(sizeof(UncvR) / 4) + "'";
        } else {
            return "(" + desc<ElementType, E>() + std::string(detail::array_dims<UncvR>()) + ")";
        }
    } else if constexpr (constexpr std::string_view scalar = detail::np_scalar_type<UncvR>(); not scalar.empty()) {
        if (scalar == "b1" or scalar == "S1" or scalar == "i1" or scalar == "u1") {
            return "'" + std::string(scalar) + "'";
        } else {
            return "'" + std::string(detail::endian_prefix<E>()) + std::string(scalar) + "'";
        }
    } else if constexpr (std::is_union_v<UncvR> or std::is_class_v<UncvR>) {
        constexpr auto ctx = std::meta::access_context::unchecked();
        static_assert(std::meta::bases_of(^^UncvR, ctx).empty(), "Inheritance is not supported for dtype extraction");

        static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(^^UncvR, ctx));

        if constexpr (not detail::explicit_layout<UncvR>()) {  // default numpy layout
            std::string list_format = "[";
            bool is_first = true;

            template for (constexpr std::meta::info member : members) {
                constexpr std::string_view field_name = std::meta::identifier_of(member);
                using FieldType = typename[:std::meta::type_of(member):];

                if (not is_first) {
                    list_format += ", ";
                }

                is_first = false;
                list_format += "('" + std::string(field_name) + "', " + desc<FieldType, E>() + ")";
            }

            list_format += "]";
            return list_format;
        } else {  // explicit dict layout due to padding, alignment gaps, or anonymous fields
            std::string names = "'names': [";
            std::string formats = "'formats': [";
            std::string offsets = "'offsets': [";
            bool is_first = true;

            template for (constexpr std::meta::info member : members) {
                constexpr std::string_view field_name = std::meta::identifier_of(member);
                using FieldType = typename[:std::meta::type_of(member):];
                constexpr std::size_t base_offset = std::is_union_v<UncvR> ? 0 : std::meta::offset_of(member).bytes;

                if constexpr (std::is_union_v<FieldType> and field_name.empty()) {   // flatten anonymous unions
                    static constexpr auto union_members = std::define_static_array(std::meta::nonstatic_data_members_of(^^FieldType, ctx));

                    template for (constexpr std::meta::info u_member : union_members) {
                        constexpr std::string_view u_name = std::meta::identifier_of(u_member);
                        using UFieldType = typename[:std::meta::type_of(u_member):];

                        if (not is_first) {
                            names += ", ";
                            formats += ", ";
                            offsets += ", ";
                        }

                        is_first = false;
                        names += "'" + std::string(u_name) + "'";
                        formats += desc<UFieldType, E>();
                        offsets += std::to_string(base_offset);
                    }
                } else {
                    if (not is_first) {
                        names += ", ";
                        formats += ", ";
                        offsets += ", ";
                    }

                    is_first = false;
                    names += "'" + std::string(field_name) + "'";
                    formats += desc<FieldType, E>();
                    offsets += std::to_string(base_offset);
                }
            }

            names += "]";
            formats += "]";
            offsets += "]";
            return "{" + names + ", " + formats + ", " + offsets + ", 'itemsize': " + std::to_string(sizeof(UncvR)) + "}";
        }
    } else {
        static_assert(detail::always_false<UncvR>::value, "Unsupported member type found during dtype conversion");
    }
}

} // namespace pydtype