#include "acv_830d68f0.hpp"
#include "constants_830d68f0.hpp"
#include "host.hpp"
#include <algorithm>
#include <cfenv>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <random>
#include <xmmintrin.h>

PPC_EXTERN_FUNC(sub_830D68F0);
PPC_EXTERN_FUNC(sub_830D69D0);
int main() {
    try {
        if (std::fesetround(FE_TONEAREST) || std::fegetround() != FE_TONEAREST)
            throw std::runtime_error("rounding setup failed");
        const unsigned mxcsr = _mm_getcsr() & ~(0x8000u | 0x0040u);
        _mm_setcsr(mxcsr);
        acv_probe::Memory memory;
        acv_probe::MemoryScope scope(memory);
        uint8_t* base = memory.base();
        memory.commit(0x70000000, memory.page_size());
        memory.commit(0x835A6000, memory.page_size());
        memory.commit(0x835A7000, memory.page_size());
        memory.commit(0x82001000, memory.page_size());
        for (const auto& [address, byte] : constants_bytes_830d68f0)
            PPC_STORE_U8(address, byte);
        uint64_t cases = 0, split_mismatches = 0, arithmetic_differences = 0;
        uint64_t fused_sample_input = 0, fused_sample_split = 0, fused_sample_fused = 0;
        double max_split_sin_error = 0, max_fused_sin_error = 0;
        uint64_t cosine_mismatches = 0, cosine_arithmetic_differences = 0;
        double max_split_cos_error = 0, max_fused_cos_error = 0;
        auto check = [&](double input) {
            _mm_setcsr(mxcsr);
            PPCContext ctx{};
            ctx.r1.u64 = 0x70000800;
            ctx.lr = 0x823025B4;
            ctx.f1.f64 = input;
            ctx.f30.u64 = 0x0123456789ABCDEF;
            ctx.f31.u64 = 0xFEDCBA9876543210;
            sub_830D68F0(ctx, base);
            const double translated = ctx.f1.f64;
            _mm_setcsr(mxcsr);
            const double split = acv_decomp::scalar_830d68f0(input, constants_830d68f0,
                acv_decomp::ScalarArithmetic::GeneratedSplit);
            const double fused = acv_decomp::scalar_830d68f0(input, constants_830d68f0,
                acv_decomp::ScalarArithmetic::FusedCandidate);
            if (!std::isfinite(translated) || !std::isfinite(split) || !std::isfinite(fused))
                throw std::runtime_error("nonfinite sine-like output in finite test domain");
            ++cases;
            if (std::bit_cast<uint64_t>(translated) != std::bit_cast<uint64_t>(split))
                ++split_mismatches;
            if (std::bit_cast<uint64_t>(fused) != std::bit_cast<uint64_t>(split)) {
                if (arithmetic_differences++ == 0) {
                    fused_sample_input = std::bit_cast<uint64_t>(input);
                    fused_sample_split = std::bit_cast<uint64_t>(split);
                    fused_sample_fused = std::bit_cast<uint64_t>(fused);
                }
            }
            if (ctx.r1.u64 != 0x70000800 || ctx.lr != 0x823025B4 ||
                ctx.f30.u64 != 0x0123456789ABCDEF || ctx.f31.u64 != 0xFEDCBA9876543210)
                throw std::runtime_error("generated preserved state mismatch");
            const double expected = std::sin(input); // independent host-libm check only
            max_split_sin_error = std::max(max_split_sin_error, std::abs(split - expected));
            max_fused_sin_error = std::max(max_fused_sin_error, std::abs(fused - expected));
            ctx.f1.f64 = input;
            sub_830D69D0(ctx, base);
            const double translated_cos = ctx.f1.f64;
            _mm_setcsr(mxcsr);
            const double split_cos = acv_decomp::scalar_830d69d0(input, constants_830d68f0,
                half_pi_830d69d0, acv_decomp::ScalarArithmetic::GeneratedSplit);
            const double fused_cos = acv_decomp::scalar_830d69d0(input, constants_830d68f0,
                half_pi_830d69d0, acv_decomp::ScalarArithmetic::FusedCandidate);
            if (!std::isfinite(translated_cos) || !std::isfinite(split_cos) || !std::isfinite(fused_cos))
                throw std::runtime_error("nonfinite cosine-like output in finite test domain");
            if (std::bit_cast<uint64_t>(translated_cos) != std::bit_cast<uint64_t>(split_cos))
                ++cosine_mismatches;
            if (std::bit_cast<uint64_t>(fused_cos) != std::bit_cast<uint64_t>(split_cos))
                ++cosine_arithmetic_differences;
            if (ctx.r1.u64 != 0x70000800 || ctx.lr != 0x823025B4 ||
                ctx.f30.u64 != 0x0123456789ABCDEF || ctx.f31.u64 != 0xFEDCBA9876543210)
                throw std::runtime_error("cosine preserved state mismatch");
            const double expected_cos = std::cos(input);
            max_split_cos_error = std::max(max_split_cos_error, std::abs(split_cos - expected_cos));
            max_fused_cos_error = std::max(max_fused_cos_error, std::abs(fused_cos - expected_cos));
        };
        // Signed zeros, subnormal bits, integer-period and half-period neighbors.
        for (uint64_t bits : {UINT64_C(0), UINT64_C(0x8000000000000000), UINT64_C(1),
                              UINT64_C(0x8000000000000001), UINT64_C(0x0010000000000000)})
            check(std::bit_cast<double>(bits));
        for (int i = -64; i <= 64; ++i) {
            const double x = double(i) * (std::numbers::pi / 2.0);
            check(x);
            check(std::nextafter(x, INFINITY));
            check(std::nextafter(x, -INFINITY));
        }
        std::mt19937_64 random(0x830D68F0);
        for (unsigned i = 0; i < 40000; ++i) {
            const double unit = double(random() >> 11) * 0x1p-53;
            const double radius = (i % 2) ? 1000000.0 : 100.0;
            check((unit * 2.0 - 1.0) * radius);
        }
        unsigned rejected = 0;
        for (double x : {INFINITY * 1.0, -INFINITY * 1.0, NAN * 1.0,
                         constants_830d68f0.limit, -constants_830d68f0.limit}) {
            try { acv_decomp::scalar_830d68f0(x, constants_830d68f0,
                    acv_decomp::ScalarArithmetic::GeneratedSplit); }
            catch (const std::domain_error&) { ++rejected; }
            try { acv_decomp::scalar_830d69d0(x, constants_830d68f0, half_pi_830d69d0,
                    acv_decomp::ScalarArithmetic::GeneratedSplit); }
            catch (const std::domain_error&) { ++rejected; }
        }
        if (split_mismatches || cosine_mismatches || rejected != 10 ||
            max_split_sin_error > 8e-15 || max_fused_sin_error > 8e-15 ||
            max_split_cos_error > 8e-15 || max_fused_cos_error > 8e-15)
            throw std::runtime_error("bounded numeric contract failed");
        std::cout << std::setprecision(17)
            << "{\"cases\":" << cases << ",\"split_mismatches\":" << split_mismatches
            << ",\"arithmetic_differences\":" << arithmetic_differences
            << ",\"rejected_out_of_scope\":" << rejected
            << ",\"max_split_vs_host_sin\":" << max_split_sin_error
            << ",\"max_fused_vs_host_sin\":" << max_fused_sin_error
            << ",\"cosine_split_mismatches\":" << cosine_mismatches
            << ",\"cosine_arithmetic_differences\":" << cosine_arithmetic_differences
            << ",\"max_split_vs_host_cos\":" << max_split_cos_error
            << ",\"max_fused_vs_host_cos\":" << max_fused_cos_error
            << ",\"sample_input_bits\":\"" << std::hex << fused_sample_input
            << "\",\"sample_split_bits\":\"" << fused_sample_split
            << "\",\"sample_fused_bits\":\"" << fused_sample_fused << "\"}\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
