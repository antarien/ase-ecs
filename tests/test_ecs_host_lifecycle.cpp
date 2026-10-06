/**
 * ASE CORE INFRASTRUCTURE IMPLEMENTATION
 *
 * @file        test_ecs_host_lifecycle.cpp
 * @design      DSGN_021
 * @brief       Pins the host-owned lifecycle of App: no signal ownership, no run(), sinks handed back
 * @description An App embedded in a host (APP_LIFE_HOST, Godot in the Vivarium client,
 *              PLAN_ASE_VIVARIUM_PHASE_00_CONTRACT section 00.1) is driven through
 *              startup/tick/shutdown by the host and must not take what belongs to the host:
 *
 *              1. THE SIGNAL MASK. A process-owned App blocks SIGINT, SIGTERM, SIGHUP and SIGPIPE
 *                 for the whole process and collects them through a signalfd. In a host those
 *                 signals belong to the host and its platform; the mask has to be the same before
 *                 startup, after startup and after shutdown.
 *              2. THE PROCESS. run() spins its own loop and ends with _exit(0). A host-owned App
 *                 refuses it with an error line and stays drivable - reaching the line after the
 *                 call is the proof that the process did not end.
 *              3. THE LOGGER. A tier server ends right after shutdown and drops the log sinks on
 *                 purpose. A host lives on: a line written after shutdown still has to reach the
 *                 host's sink.
 *
 *              EVERY CHECK HAS ITS POSITIVE CONTROL IN THIS FILE. The mask probe is shown to see
 *              a change on the process-owned path (SIGINT blocked after startup), and the sink
 *              probe is shown to miss a line on the process-owned path (sinks dropped). Without
 *              them a probe that can never see anything would pass every host case.
 *
 *              ITS OWN TARGET, NOT A SECOND SOURCE OF ase-ecs-test: that target switches the
 *              doctest entry point on for every translation unit, and a second source would bring
 *              a second main() that breaks only at link time (tests/CMakeLists.txt says so).
 *
 * @module      ase-ecs
 * @layer       1 (Core)
 * @category    process/validation/check
 * @created     2026-10-05
 * @modified    2026-10-05
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

#include <doctest/doctest.h>

#include <ase/ecs/app.hpp>
#include <ase/ecs/system.hpp>
#include <ase/ecs/components/app/state/ecs_app_sta_shtd_comp.hpp>
#include <ase/log/log.hpp>
#include <ase/log/log_lifecycle.hpp>

#include <csignal>
#include <cstdint>
#include <string_view>

// POSIX: pthread_sigmask with a null set only READS the calling thread's mask.
#include <pthread.h>

namespace ase::ecs {

// ============================================================================
// Test fixtures
// ============================================================================

// How often each lifecycle schedule reached the App. One row on one entity; the two systems
// below count into it, so neither the App nor a system carries test state.
struct HostLifeRunsComponent {
    uint32_t initialization = 0;
    uint32_t finalization = 0;
};

// How many on_stop calls had run, and how often the stopped callback fired and what it saw.
struct HostLifeStopsComponent {
    uint32_t stops = 0;
    uint32_t stops_seen_by_callback = 0;
    uint32_t callbacks = 0;
};

// What the host's log sink received: every line, and every line carrying `marker`.
struct HostSinkProbe {
    const char* marker = nullptr;
    uint32_t lines = 0;
    uint32_t marked = 0;
};

class HostLifeInitSystem : public ecs::System {
public:
    const char* name() const override { return "HostLifeInitSystem"; }

    void tick(Registry& registry, float dt) override {
        (void)dt;
        for (auto [entity, runs] : registry.view<HostLifeRunsComponent>().each()) {
            (void)entity;
            runs.initialization += 1u;
        }
    }
};

class HostLifeFiniSystem : public ecs::System {
public:
    const char* name() const override { return "HostLifeFiniSystem"; }

    void tick(Registry& registry, float dt) override {
        (void)dt;
        for (auto [entity, runs] : registry.view<HostLifeRunsComponent>().each()) {
            (void)entity;
            runs.finalization += 1u;
        }
    }
};

// Counts its own on_stop into every HostLifeStopsComponent row; registered in several schedules.
class HostLifeStopCountSystem : public ecs::System {
public:
    const char* name() const override { return "HostLifeStopCountSystem"; }

    void tick(Registry& registry, float dt) override {
        (void)registry;
        (void)dt;
    }

    void on_stop(Registry& registry) override {
        for (auto [entity, stops] : registry.view<HostLifeStopsComponent>().each()) {
            (void)entity;
            stops.stops += 1u;
        }
    }
};

namespace {

// The stopped callback: records how many on_stop calls it saw, and that it ran.
void record_stopped(App& app) {
    for (auto [entity, stops] : app.registry().view<HostLifeStopsComponent>().each()) {
        (void)entity;
        stops.stops_seen_by_callback = stops.stops;
        stops.callbacks += 1u;
    }
}

// true when the calling thread currently blocks `signo`.
bool blocked(int signo) {
    sigset_t current;
    sigemptyset(&current);
    pthread_sigmask(SIG_SETMASK, nullptr, &current);
    return sigismember(&current, signo) == 1;
}

// The host's log sink: counts every line and every line that carries the probe's marker. The
// logger is synchronous (ase-log builds a plain spdlog::logger), so a line has been counted
// when the log call returns.
void count_host_line(const char* line, uint32_t len, int level, void* user) {
    (void)level;
    auto* probe = static_cast<HostSinkProbe*>(user);
    probe->lines += 1u;
    if (std::string_view(line, len).find(probe->marker) != std::string_view::npos) {
        probe->marked += 1u;
    }
}

}  // anonymous namespace

// ============================================================================
// Test cases
// ============================================================================

TEST_CASE("host-owned App leaves the signal mask as it found it through startup, tick and shutdown") {
    const bool int_before  = blocked(SIGINT);
    const bool term_before = blocked(SIGTERM);
    const bool hup_before  = blocked(SIGHUP);
    const bool pipe_before = blocked(SIGPIPE);

    App app(APP_LIFE_HOST);
    CHECK(app.lifecycle() == APP_LIFE_HOST);

    app.startup();
    CHECK(blocked(SIGINT) == int_before);
    CHECK(blocked(SIGTERM) == term_before);
    CHECK(blocked(SIGHUP) == hup_before);
    CHECK(blocked(SIGPIPE) == pipe_before);
    // No watch armed: without the component tick() reads no signal and shutdown() closes no
    // descriptor this App never opened.
    CHECK(app.registry().storage<EcsAppStaShtdComponent>().size() == 0u);

    app.tick(0.125f);
    app.shutdown();
    CHECK(blocked(SIGINT) == int_before);
    CHECK(blocked(SIGTERM) == term_before);
    CHECK(blocked(SIGHUP) == hup_before);
    CHECK(blocked(SIGPIPE) == pipe_before);
}

TEST_CASE("host-owned App runs Initialization once at startup and Finalization once at shutdown") {
    App app(APP_LIFE_HOST);
    app.add_system<HostLifeInitSystem>(Schedule::Initialization);
    app.add_system<HostLifeFiniSystem>(Schedule::Finalization);

    Registry& registry = app.registry();
    const auto row = registry.create();
    registry.emplace<HostLifeRunsComponent>(row);

    app.startup();
    CHECK(registry.get<HostLifeRunsComponent>(row).initialization == 1u);
    CHECK(registry.get<HostLifeRunsComponent>(row).finalization == 0u);

    // Four steps of 0.125 s make one Regulation step; neither lifecycle schedule runs again.
    app.tick(0.125f);
    app.tick(0.125f);
    app.tick(0.125f);
    app.tick(0.125f);
    CHECK(registry.get<HostLifeRunsComponent>(row).initialization == 1u);
    CHECK(registry.get<HostLifeRunsComponent>(row).finalization == 0u);

    app.shutdown();
    CHECK(registry.get<HostLifeRunsComponent>(row).initialization == 1u);
    CHECK(registry.get<HostLifeRunsComponent>(row).finalization == 1u);
}

TEST_CASE("run() on a host-owned App is refused and leaves the App drivable") {
    App app(APP_LIFE_HOST);
    app.add_system<HostLifeInitSystem>(Schedule::Initialization);

    Registry& registry = app.registry();
    const auto row = registry.create();
    registry.emplace<HostLifeRunsComponent>(row);

    // A process-owned App would never come back from this call: run() ends with _exit(0).
    app.run();

    // run() did not even start the App - the host still owns that step.
    CHECK(registry.get<HostLifeRunsComponent>(row).initialization == 0u);
    CHECK(registry.storage<EcsAppStaShtdComponent>().size() == 0u);

    app.startup();
    CHECK(registry.get<HostLifeRunsComponent>(row).initialization == 1u);
    app.shutdown();
}

TEST_CASE("twenty host-owned Apps in a row each boot once, stop once and leave the mask untouched") {
    constexpr uint32_t HostRestarts = 20u;

    const bool int_before  = blocked(SIGINT);
    const bool pipe_before = blocked(SIGPIPE);

    for (uint32_t cycle = 0u; cycle < HostRestarts; ++cycle) {
        App app(APP_LIFE_HOST);
        app.add_system<HostLifeInitSystem>(Schedule::Initialization);
        app.add_system<HostLifeFiniSystem>(Schedule::Finalization);

        Registry& registry = app.registry();
        const auto row = registry.create();
        registry.emplace<HostLifeRunsComponent>(row);

        app.startup();
        app.tick(0.125f);
        app.tick(0.125f);
        app.tick(0.125f);
        app.tick(0.125f);
        app.shutdown();

        CHECK(app.system_count() == 2u);
        CHECK(registry.get<HostLifeRunsComponent>(row).initialization == 1u);
        CHECK(registry.get<HostLifeRunsComponent>(row).finalization == 1u);
        CHECK(blocked(SIGINT) == int_before);
        CHECK(blocked(SIGPIPE) == pipe_before);
    }
}

TEST_CASE("host-owned App hands the log sinks back: a line after shutdown still reaches the host") {
    HostSinkProbe probe;
    probe.marker = "host-lifecycle-marker-after-shutdown";
    log::init_tui_standalone("ase-ecs-host-test", "TEST", "", &count_host_line, &probe);

    App app(APP_LIFE_HOST);
    app.startup();
    app.tick(0.125f);
    app.shutdown();

    const uint32_t marked_after_host = probe.marked;
    log::info("[HostLifeTest] {}", probe.marker);
    CHECK(probe.marked == marked_after_host + 1u);

    // The refusal of run() is a visible error line at the host's sink, not a silent no-op.
    const uint32_t lines_before_refusal = probe.lines;
    App refused(APP_LIFE_HOST);
    refused.run();
    CHECK(probe.lines > lines_before_refusal);

    // POSITIVE CONTROL: the process-owned path drops the sinks on purpose (the tier ends with
    // _exit right after). The same line written after ITS shutdown must not arrive - otherwise
    // the check above would pass whether or not the host path hands anything back.
    {
        App process_owned;
        process_owned.startup();
        process_owned.shutdown();
    }
    const uint32_t marked_after_process = probe.marked;
    log::info("[HostLifeTest] {}", probe.marker);
    CHECK(probe.marked == marked_after_process);

    // The probe lives on this stack; the logger must not outlive it.
    log::shutdown();
}

TEST_CASE("the stopped callback fires once, after the on_stop of every system") {
    App app(APP_LIFE_HOST);
    app.add_system<HostLifeStopCountSystem>(Schedule::Initialization);
    app.add_system<HostLifeStopCountSystem>(Schedule::Regulation);
    app.add_system<HostLifeStopCountSystem>(Schedule::Finalization);
    app.set_on_stopped(&record_stopped);

    Registry& registry = app.registry();
    const auto row = registry.create();
    registry.emplace<HostLifeStopsComponent>(row);

    app.startup();
    app.tick(0.125f);
    // POSITIVE CONTROL: nothing has stopped and the callback has not fired before shutdown.
    CHECK(registry.get<HostLifeStopsComponent>(row).stops == 0u);
    CHECK(registry.get<HostLifeStopsComponent>(row).callbacks == 0u);

    app.shutdown();
    const auto& stops = registry.get<HostLifeStopsComponent>(row);
    CHECK(stops.stops == 3u);
    CHECK(stops.callbacks == 1u);
    // All three on_stop had returned when the callback ran - whatever schedule they sit in.
    CHECK(stops.stops_seen_by_callback == 3u);
}

TEST_CASE("the default constructor keeps the process-owned lifecycle and arms its signal watch") {
    App app;
    CHECK(app.lifecycle() == APP_LIFE_PROCESS);

    // POSITIVE CONTROL for blocked(): the process-owned startup blocks SIGINT for the process,
    // so the probe the host cases rely on demonstrably sees a change of the mask.
    app.startup();
    CHECK(blocked(SIGINT));
    CHECK(app.registry().storage<EcsAppStaShtdComponent>().size() == 1u);

    app.shutdown();
    CHECK(!blocked(SIGINT));
    // SIGPIPE stays blocked by design (app.cpp, close_shutdown_watch).
    CHECK(blocked(SIGPIPE));
    CHECK(app.registry().storage<EcsAppStaShtdComponent>().size() == 0u);
}

}  // namespace ase::ecs
