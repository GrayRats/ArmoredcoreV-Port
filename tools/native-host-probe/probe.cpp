#include "host.hpp"
#include <iostream>

PPC_EXTERN_FUNC(sub_822B0818);
PPC_EXTERN_FUNC(sub_822B0820);

int main() {
    try {
        unsigned checks = 0;
        auto require = [&](bool ok) { if (!ok) throw std::runtime_error("probe assertion failed"); ++checks; };
        auto rejects = [&](auto action) {
            bool rejected = false;
            try { action(); } catch (const std::out_of_range&) { rejected = true; }
            require(rejected);
        };
        acv_probe::Memory memory;
        acv_probe::MemoryScope memory_scope(memory);
        acv_probe::Dispatcher dispatcher;
        acv_probe::DispatcherScope dispatcher_scope(dispatcher);
        dispatcher.add(0x822B0818, sub_822B0818);
        dispatcher.add(0x822B0820, sub_822B0820);
        uint8_t* base = memory.base();
        constexpr uint32_t page = 0x83885000;
        memory.commit(page, memory.page_size());
        PPCContext ctx{};
        for (uint32_t value : {0u, 1u, 0x12345678u, 0x80000000u, 0xffffffffu}) {
            ctx.r3.u64 = page + 16;
            ctx.r4.u64 = value;
            ctx.lr = 0xabcdef00;
            PPC_CALL_INDIRECT_FUNC(0x822B0818);
            require(PPC_LOAD_U32(page + 16) == value);
            auto bytes = memory.checked(base, page + 16, 4);
            require(bytes[0] == uint8_t(value >> 24) && bytes[3] == uint8_t(value));
            require(ctx.r3.u64 == page + 16 && ctx.r4.u64 == value && ctx.lr == 0xabcdef00);
            PPC_CALL_INDIRECT_FUNC(0x822B0820);
            require(PPC_LOAD_U32(page + 16) == 0 && ctx.r11.u64 == 0);
        }
        PPC_STORE_U64(page + 32, uint64_t{0x0123456789abcdef});
        require(PPC_LOAD_U64(page + 32) == 0x0123456789abcdef);
        PPC_STORE_U16(page + 41, uint16_t{0xfedc}); // unaligned, checked byte path
        require(PPC_LOAD_U16(page + 41) == 0xfedc);
        PPC_STORE_U8(page + 43, uint8_t{0x81});
        require(PPC_LOAD_U8(page + 43) == 0x81);
        rejects([&] { PPC_LOAD_U32(page - 4); });
        rejects([&] { PPC_STORE_U32(page + memory.page_size() - 2, 0u); });
        rejects([&] { PPC_LOAD_U32(0xffffffffull); });
        rejects([&] { PPC_LOAD_U8(0x100000000ull); });
        rejects([&] { acv_probe::load<uint32_t>(base + 32, page); });
        rejects([&] { PPC_CALL_INDIRECT_FUNC(0x822B0824); });
        rejects([&] { PPC_CALL_INDIRECT_FUNC(0x822B0819); });
        rejects([&] { PPC_CALL_INDIRECT_FUNC(0x1822B0818ull); });
        ctx.r3.u64 = page - 4;
        rejects([&] { PPC_CALL_INDIRECT_FUNC(0x822B0818); });
        rejects([&] { memory.commit(page, 0); });
        rejects([&] { memory.commit(page + 1, memory.page_size()); });
        rejects([&] { memory.commit(page, memory.page_size() - 1); });
        rejects([&] { memory.commit(0x100000000ull, memory.page_size()); });
        rejects([&] { memory.commit(0x100000000ull - memory.page_size(), 2 * memory.page_size()); });
        // Exercise the top of the 32-bit range and recommitting live pages.
        memory.commit(0x100000000ull - memory.page_size(), memory.page_size());
        PPC_STORE_U32(0xfffffffcull, 0x01234567u);
        require(PPC_LOAD_U32(0xfffffffcull) == 0x01234567u);
        memory.commit(0x100000000ull - memory.page_size(), memory.page_size());
        require(PPC_LOAD_U32(0xfffffffcull) == 0x01234567u);
        rejects([&] { PPC_LOAD_U64(0xfffffffcull); });
        memory.commit(page + memory.page_size(), memory.page_size());
        PPC_STORE_U64(page + memory.page_size() - 4, 0xfedcba9876543210ull);
        require(PPC_LOAD_U64(page + memory.page_size() - 4) == 0xfedcba9876543210ull);
        std::cout << "{\"checks\":" << checks
                  << ",\"generated_functions\":2,\"guest_page\":\"0x83885000\",\"status\":\"passed\"}\n";
    } catch (const std::exception& e) {
        std::cerr << "probe failed: " << e.what() << '\n';
        return 1;
    }
}
