/**
 * ASE CORE INFRASTRUCTURE IMPLEMENTATION
 *
 * @file        dependency_sorter.cpp
 * @brief       Topological sort for system dependencies
 * @description Orders the systems inside one schedule by their run_after
 *              declarations using Kahn's algorithm, and reports an error when
 *              the declarations form a cycle. Systems with no incoming edge
 *              keep their registration order - that orders them in practice
 *              but promises nothing, so a required order needs run_after.
 *              Internal implementation detail of ase-ecs.
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @design      DSGN_016
 * @category    process/computation/algorithm
 * @created     2026-02-01
 * @modified    2026-08-20
 * @version     1.0.0
 *
 * CORE INFRASTRUCTURE IMPLEMENTATION COMPLIANCE
 *
 * [ ] NOT an ECS System implementation
 * [ ] Layer dependencies correct (L0: no ASE deps, L1: L0 only)
 * [ ] Own header included FIRST
 * [ ] No global mutable state
 * [ ] No static initialization order fiasco
 * [ ] Thread-safe implementations (pure or mutex-protected)
 * [ ] All error conditions handled
 * [ ] No exceptions thrown (use Result<T> pattern)
 * [ ] Implementation details in anonymous namespace
 * [ ] No inline implementations of template specializations here
 * [ ] Platform-specific code isolated and documented
 * [ ] Performance-critical code profiled and optimized
 */

#include <ase/ecs/internal/dependency_sorter.hpp>
#include <ase/log/log.hpp>

#include <ase/containers/hash_map.hpp>
#include <ase/containers/ring_buffer.hpp>
#include <ase/containers/vector.hpp>

namespace ase::ecs::internal {

ase::containers::Vector<CycleError> sort_systems_by_dependencies(SystemRegistry& registry) {
    ase::containers::Vector<CycleError> errors;

    for (auto& [schedule, systems] : registry.all_systems()) {
        if (systems.size() <= 1) continue;

        // Build name → index mapping
        ase::containers::HashMap<std::string, size_t> name_to_idx;
        for (size_t i = 0; i < systems.size(); ++i) {
            if (systems[i]) {
                name_to_idx[systems[i]->name()] = i;
            }
        }

        // Find matching SystemInfo for each system
        ase::containers::HashMap<std::string, const SystemInfo*> info_map;
        for (const auto& info : registry.infos()) {
            if (info.schedule == schedule) {
                info_map[info.name] = &info;
            }
        }

        // Build adjacency list and in-degree
        ase::containers::Vector<ase::containers::Vector<size_t>> adj(systems.size());
        ase::containers::Vector<int> in_degree(systems.size(), 0);

        for (size_t i = 0; i < systems.size(); ++i) {
            if (!systems[i]) continue;

            auto it = info_map.find(systems[i]->name());
            if (it == info_map.end()) continue;

            const auto* info = it->second;
            for (const auto& dep : info->run_after) {
                auto dep_it = name_to_idx.find(dep);
                if (dep_it != name_to_idx.end()) {
                    // dep must run before i, so edge from dep → i
                    adj[dep_it->second].push_back(i);
                    in_degree[i]++;
                }
            }
        }

        // Kahn's algorithm: store INDICES, not pointers!
        // This is the FIX: we don't move any pointers until we confirm no cycle.
        //
        // RingBuffer is the containers SSOT for a FIFO, but its capacity is a
        // COMPILE-TIME bound and push() returns false when it is reached. A
        // silently dropped index would not crash - it would produce a shorter
        // sorted list, which the cycle check below then reports as a CYCLE.
        // Every push is therefore checked, and an overflow says what it is.
        // The bound is the containers default (1024) against ~1088 systems in
        // the WHOLE tree spread over 66 schedules; one schedule stays far
        // below it, and the check is what makes that a fact rather than a hope.
        ase::containers::RingBuffer<size_t> queue;
        bool queue_overflow = false;
        for (size_t i = 0; i < systems.size(); ++i) {
            if (in_degree[i] == 0) {
                if (!queue.push(i)) queue_overflow = true;
            }
        }

        ase::containers::Vector<size_t> sorted_indices;
        sorted_indices.reserve(systems.size());

        while (!queue.empty()) {
            // pop() writes into `u` and reports whether it did (umgestellt 2026-08-20, vorher
            // std::optional). Das leere Ziel bleibt unberuehrt, wenn der Puffer leer ist.
            size_t u = 0;
            if (!queue.pop(u)) break;
            sorted_indices.push_back(u);

            for (size_t v : adj[u]) {
                if (--in_degree[v] == 0) {
                    if (!queue.push(v)) queue_overflow = true;
                }
            }
        }

        if (queue_overflow) {
            // Die Wertform OHNE Besitzer: hier gibt es keine Entity, und `queue.push()` meldet
            // nur `false` — die RingBuffer-Kapazitaet ist an diesem Punkt nicht als Zahl
            // greifbar. Deshalb die Form mit EINEM Wert und nicht die 7-Argument-Form mit
            // min/max: eine Grenze zu erfinden waere eine Falschaussage im strukturierten Feld.
            //
            // WAS DIE ZEILE NICHT MEHR SAGT und der Log-Leser wissen muss: die Sortierung
            // unterhalb ist danach UNVOLLSTAENDIG und wird als Zyklus gemeldet. Die
            // Kategorie-Formen tragen value_id + Wert ODER value_id + Text, nie beides —
            // die Folge steht deshalb hier und nicht mehr im Log. Wer den Zyklus-Befund
            // unten sieht, pruefe ZUERST, ob direkt davor diese Zeile steht: dann ist es
            // kein Zyklus, sondern ein Ueberlauf.
            log::error(log::ERR::CAT::CAPACITY_REACHED, "DependencySorter", "ready_queue",
                       static_cast<float>(systems.size()));
        }

        // Check for cycle
        if (sorted_indices.size() != systems.size()) {
            // Cycle detected! Find participants (nodes with remaining in-degree)
            CycleError error;
            error.schedule = schedule;
            for (size_t i = 0; i < systems.size(); ++i) {
                if (in_degree[i] > 0 && systems[i]) {
                    error.cycle_participants.push_back(systems[i]->name());
                }
            }

            // value_id ist der Schedule-Name — der bleibt je Aufrufstelle stabil und ist damit
            // der Filterschluessel. Der Zyklusteilnehmer wechselt und gehoert deshalb in
            // `detail`, nicht in value_id: ein wechselnder Wert dort machte jede Zeile zu einem
            // eigenen Schluessel.
            log::error(log::ERR::CAT::SCHEDULE_ORDER, "DependencySorter",
                       schedule_name(schedule),
                       error.cycle_participants.empty() ? "unknown"
                                                        : error.cycle_participants[0].c_str());

            errors.push_back(std::move(error));
            // Keep original order: systems vector is UNCHANGED (no nulls!)
            continue;
        }

        // No cycle: apply the permutation to BOTH schedule_systems_ (tick loop)
        // AND schedule_info_indices_ (boot logger) atomically, so on_start order
        // matches tick order. Previously only schedule_systems_ was reordered,
        // which caused run_after to be silently ignored during boot.
        registry.reorder_schedule(schedule, sorted_indices);
    }

    return errors;
}

}  // namespace ase::ecs::internal
