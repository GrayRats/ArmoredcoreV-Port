// Host SDK algorithm experiment; all addresses below are synthetic, not retail evidence.
#include <rex/codegen/function_graph.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace rex::codegen;
uint64_t digest = 14695981039346656037ull;
void mix(uint64_t value) { digest ^= value; digest *= 1099511628211ull; }
void snapshot(const FunctionGraph& graph) {
    std::vector<uint32_t> entries;
    for (const auto& [base, _] : graph.functions()) entries.push_back(base);
    std::sort(entries.begin(), entries.end());
    mix(entries.size());
    for (auto base : entries) {
        auto* node = graph.getFunction(base);
        mix(base); mix(node->size()); mix(uint8_t(node->authority())); mix(uint8_t(node->state()));
        for (auto* edges : {&node->calls(), &node->tailCalls()}) {
            mix(edges->size());
            for (const auto& edge : *edges) {
                mix(edge.site); mix(edge.target.value.index());
                if (auto* function = edge.target.asFunction()) mix(function->base());
                else if (auto* import = std::get_if<CallTarget::ToImport>(&edge.target.value)) mix(import->address);
                else mix(std::get<CallTarget::Unresolved>(edge.target.value).address);
            }
        }
        mix(node->unresolvedJumps().size());
        for (const auto& jump : node->unresolvedJumps()) {
            mix(jump.site); mix(jump.target); mix(jump.isCall); mix(jump.isConditional);
        }
    }
}
int main(int argc, char** argv) {
    const bool bench = argc == 2 && std::string(argv[1]) == "bench";
    const auto start = std::chrono::steady_clock::now();
    FunctionGraph graph;
    unsigned steps = 0;
    if (bench) {
        for (unsigned i = 0; i < 20000; ++i) {
            auto base = 0x82000000u + i * 32;
            graph.addFunction(base, 32, FunctionAuthority::PDATA);
            // Half wait on a not-yet-known target. Many sites may share a target.
            if (!(i & 1)) graph.addUnresolvedJumpToFunction(base, base + 4,
                0x83000000u + (i % 1024) * 32, false, false);
            ++steps;
        }
        for (unsigned i = 0; i < 1024; ++i) {
            graph.addFunction(0x83000000u + i * 32, 32, FunctionAuthority::GAP_FILL);
            ++steps;
        }
    } else {
        uint64_t random = 0x123456789abcdef0ull;
        for (unsigned i = 0; i < 5000; ++i) {
            random ^= random << 13; random ^= random >> 7; random ^= random << 17;
            const auto owner = 0x82000000u + unsigned(random % 128) * 32;
            const auto target = 0x83000000u + unsigned((random >> 16) % 128) * 32;
            switch ((random >> 32) % 5) {
                case 0: graph.removeFunction(owner); graph.addFunction(owner, 32, FunctionAuthority::DISCOVERED); break;
                case 1: graph.addFunction(owner, 32, FunctionAuthority::CONFIG); break;
                case 2: graph.addFunction(target, 32, FunctionAuthority::PDATA); break;
                default:
                    if (!graph.getFunction(owner)) graph.addFunction(owner, 32, FunctionAuthority::PDATA);
                    graph.addUnresolvedJumpToFunction(owner, owner + unsigned((random >> 48) % 8) * 4,
                                                     target, bool(random & 1), bool(random & 2));
                    break;
            }
            snapshot(graph);
            ++steps;
        }
        // Explicit stale-index case: remove/reuse an owner while targets remain unknown.
        constexpr uint32_t owner = 0x82100000, a = 0x83100000, b = 0x83100020;
        graph.addFunction(owner, 32, FunctionAuthority::PDATA);
        graph.addUnresolvedJumpToFunction(owner, owner + 4, a, false, false);
        graph.removeFunction(owner);
        graph.addFunction(owner, 32, FunctionAuthority::CONFIG);
        graph.addUnresolvedJumpToFunction(owner, owner + 8, b, false, false);
        graph.addFunction(a, 32, FunctionAuthority::GAP_FILL);
        if (graph.getFunction(owner)->unresolvedJumps().size() != 1)
            throw std::runtime_error("stale owner resolved the wrong target");
        graph.addFunction(b, 32, FunctionAuthority::GAP_FILL);
        if (!graph.getFunction(owner)->unresolvedJumps().empty())
            throw std::runtime_error("live target was not resolved");
        steps += 7;
        // Two sites in one owner must both resolve; duplicate target insertion
        // must not create duplicate edges.
        constexpr uint32_t shared_owner = 0x82101000, shared_target = 0x83101000;
        graph.addFunction(shared_owner, 32, FunctionAuthority::PDATA);
        graph.addUnresolvedJumpToFunction(shared_owner, shared_owner + 4, shared_target, false, false);
        graph.addUnresolvedJumpToFunction(shared_owner, shared_owner + 8, shared_target, true, true);
        graph.addFunction(shared_target, 32, FunctionAuthority::PDATA);
        graph.addFunction(shared_target, 32, FunctionAuthority::PDATA);
        if (graph.getFunction(shared_owner)->tailCalls().size() != 2 ||
            !graph.getFunction(shared_owner)->unresolvedJumps().empty())
            throw std::runtime_error("shared target resolution differs");
        steps += 5;
        // A higher-authority replacement discards the old owner's jumps.
        constexpr uint32_t replaced = 0x82102000, replaced_target = 0x83102000;
        graph.addFunction(replaced, 32, FunctionAuthority::PDATA);
        graph.addUnresolvedJumpToFunction(replaced, replaced + 4, replaced_target, false, false);
        graph.addFunction(replaced, 32, FunctionAuthority::CONFIG);
        graph.addFunction(replaced_target, 32, FunctionAuthority::PDATA);
        if (!graph.getFunction(replaced)->tailCalls().empty())
            throw std::runtime_error("replaced owner's obsolete jump survived");
        steps += 4;
        // Preserve the API's existing exclusion of sealed owners from notification.
        constexpr uint32_t sealed = 0x82103000, sealed_target = 0x83103000;
        auto* sealed_node = graph.addFunction(sealed, 32, FunctionAuthority::PDATA);
        sealed_node->discover({Block{sealed, 32}}, {}, {});
        sealed_node->seal();
        graph.addUnresolvedJumpToFunction(sealed, sealed + 4, sealed_target, false, false);
        graph.addFunction(sealed_target, 32, FunctionAuthority::PDATA);
        if (graph.getFunction(sealed)->unresolvedJumps().size() != 1)
            throw std::runtime_error("sealed owner was unexpectedly notified");
        steps += 5;
        // Moving the graph keeps pending resolution attached to the moved state.
        constexpr uint32_t moved_owner = 0x82104000, moved_target = 0x83104000;
        graph.addFunction(moved_owner, 32, FunctionAuthority::PDATA);
        graph.addUnresolvedJumpToFunction(moved_owner, moved_owner + 4, moved_target, false, false);
        FunctionGraph moved = std::move(graph);
        graph = std::move(moved);
        graph.addFunction(moved_target, 32, FunctionAuthority::PDATA);
        if (!graph.getFunction(moved_owner)->unresolvedJumps().empty() ||
            graph.getFunction(moved_owner)->tailCalls().size() != 1)
            throw std::runtime_error("moved graph lost pending resolution");
        steps += 5;
    }
    snapshot(graph);
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start).count();
    std::cout << "{\"steps\":" << steps << ",\"functions\":" << graph.functionCount()
              << ",\"digest\":\"" << std::hex << digest << std::dec
              << "\",\"elapsed_us\":" << elapsed << "}\n";
}
