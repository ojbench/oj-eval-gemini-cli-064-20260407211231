#pragma once
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <exception>
#include <iostream>
#include <ostream>
#include <span>
#include <string_view>
#include <string>
#include <vector>
#include <cstdint>

namespace sjtu {

using sv_t = std::string_view;

struct format_error : std::exception {
public:
    format_error(const char *msg = "invalid format") : msg(msg) {}
    auto what() const noexcept -> const char * override {
        return msg;
    }

private:
    const char *msg;
};

template <typename Tp>
struct formatter;

struct format_info {
    inline static constexpr auto npos = static_cast<std::size_t>(-1);
    std::size_t position; // where is the specifier
    std::size_t consumed; // how many characters consumed
};

template <typename... Args>
struct format_string {
public:
    // must be constructed at compile time, to ensure the format string is valid
    consteval format_string(const char *fmt);
    constexpr auto get_format() const -> std::string_view {
        return fmt_str;
    }
    constexpr auto get_index() const -> std::span<const format_info> {
        return fmt_idx;
    }

private:
    inline static constexpr auto Nm = sizeof...(Args);
    std::string_view fmt_str;            // the format string
    std::array<format_info, Nm> fmt_idx; // where are the specifiers
};

constexpr auto find_specifier(sv_t &fmt) -> bool {
    do {
        if (const auto next = fmt.find('%'); next == sv_t::npos) {
            return false;
        } else if (next + 1 == fmt.size()) {
            throw format_error{"missing specifier after '%'"};
        } else if (fmt[next + 1] == '%') {
            // escape the specifier
            fmt.remove_prefix(next + 2);
        } else {
            fmt.remove_prefix(next + 1);
            return true;
        }
    } while (true);
};

template <typename... Args>
consteval auto compile_time_format_check(sv_t fmt, std::span<format_info> idx) -> void {
    std::size_t n = 0;
    auto parse_fn = [&fmt, &n, idx](const auto &parser) {
        const auto last_pos = fmt.begin();
        if (!find_specifier(fmt)) {
            // no specifier found
            idx[n++] = {
                .position = format_info::npos,
                .consumed = 0,
            };
            return;
        }

        const auto position = static_cast<std::size_t>(fmt.begin() - last_pos - 1);
        const auto consumed = parser.parse(fmt);

        idx[n++] = {
            .position = position,
            .consumed = consumed,
        };

        if (consumed > 0) {
            fmt.remove_prefix(consumed);
        } else if (fmt.starts_with("_")) {
            fmt.remove_prefix(1);
        } else {
            throw format_error{"invalid specifier"};
        }
    };

    (parse_fn(formatter<Args>{}), ...);
    if (find_specifier(fmt)) // extra specifier found
        throw format_error{"too many specifiers"};
}

template <typename... Args>
consteval format_string<Args...>::format_string(const char *fmt) :
    fmt_str(fmt), fmt_idx() {
    compile_time_format_check<Args...>(fmt_str, fmt_idx);
}

template <typename StrLike>
    requires(
        std::same_as<StrLike, std::string> ||      //
        std::same_as<StrLike, std::string_view> || //
        std::same_as<StrLike, char *> ||           //
        std::same_as<StrLike, const char *>        //
    )
struct formatter<StrLike> {
    static constexpr auto parse(sv_t fmt) -> std::size_t {
        return fmt.starts_with("s") ? 1 : 0;
    }
    static auto format_to(std::ostream &os, const StrLike &val, sv_t fmt = "s") -> void {
        if (fmt.starts_with("s") || fmt.starts_with("_")) {
            os << static_cast<sv_t>(val);
        } else {
            throw format_error{};
        }
    }
};

template <std::integral Int>
struct formatter<Int> {
    static constexpr auto parse(sv_t fmt) -> std::size_t {
        if constexpr (std::is_signed_v<Int>) {
            return fmt.starts_with("d") ? 1 : 0;
        } else {
            return fmt.starts_with("u") ? 1 : 0;
        }
    }
    static auto format_to(std::ostream &os, const Int &val, sv_t fmt = "") -> void {
        if constexpr (std::is_signed_v<Int>) {
            if (fmt.starts_with("d") || fmt.starts_with("_")) {
                os << static_cast<int64_t>(val);
            } else {
                throw format_error{};
            }
        } else {
            if (fmt.starts_with("u") || fmt.starts_with("_")) {
                os << static_cast<uint64_t>(val);
            } else {
                throw format_error{};
            }
        }
    }
};

template <typename T>
struct formatter<std::vector<T>> {
    static constexpr auto parse(sv_t fmt) -> std::size_t {
        return 0; // only supports %_
    }
    static auto format_to(std::ostream &os, const std::vector<T> &val, sv_t fmt = "") -> void {
        if (fmt.starts_with("_")) {
            os << "[";
            bool first = true;
            for (const auto &item : val) {
                if (!first) {
                    os << ",";
                }
                first = false;
                formatter<T>::format_to(os, item, "_");
            }
            os << "]";
        } else {
            throw format_error{};
        }
    }
};

template <typename... Args>
using format_string_t = format_string<std::decay_t<Args>...>;

template <typename... Args>
inline auto printf(format_string_t<Args...> fmt, const Args &...args) -> void {
    auto fmt_str = fmt.get_format();
    auto idx = fmt.get_index();
    
    std::size_t arg_idx = 0;
    
    auto print_arg = [&]<typename T>(const T& arg) {
        auto info = idx[arg_idx++];
        if (info.position == format_info::npos) return;
        
        // Print characters before the specifier
        for (std::size_t i = 0; i < info.position; ++i) {
            if (fmt_str[i] == '%' && i + 1 < info.position && fmt_str[i+1] == '%') {
                std::cout << '%';
                ++i; // skip the second %
            } else {
                std::cout << fmt_str[i];
            }
        }
        
        fmt_str.remove_prefix(info.position + 1); // skip up to %
        
        sv_t specifier = fmt_str;
        if (info.consumed > 0) {
            specifier = fmt_str.substr(0, info.consumed);
            fmt_str.remove_prefix(info.consumed);
        } else {
            specifier = fmt_str.substr(0, 1); // "_"
            fmt_str.remove_prefix(1);
        }
        
        formatter<std::decay_t<decltype(arg)>>::format_to(std::cout, arg, specifier);
    };
    
    (print_arg(args), ...);
    
    // Print remaining characters
    for (std::size_t i = 0; i < fmt_str.size(); ++i) {
        if (fmt_str[i] == '%' && i + 1 < fmt_str.size() && fmt_str[i+1] == '%') {
            std::cout << '%';
            ++i;
        } else {
            std::cout << fmt_str[i];
        }
    }
}

} // namespace sjtu
