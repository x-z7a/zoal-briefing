# BriefingMaster (zoal-briefing): Design Document

**Plugin:** `zoal-briefing.xpl` (signature `ZBRIEF`)
**Target:** X-Plane 12.4.4+ (SDK 4.4, needed for `XPLMPanelGraphics`) on macOS, Windows and Linux
**Language:** C++17, CMake ≥ 3.24
**License:** MIT. `zoal-atc` is GPL-v3, so we borrow its *patterns*, not its code (see §6).
**Status (2026-11-18):** Scaffolding only. The plugin loads and shows its panel. Every pipeline component is a stub or a prototype. We will build **one component at a time** (§8).

---

## 1. Goal

A pilot says the wake word **"Cookie Monster"** and asks for a briefing ("departure briefing for runway 28L"). The plugin:

1. captures the speech and transcribes it locally (whisper.cpp),
2. works out the briefing type and its parameters (airport, runway, phase),
3. gathers context: X-Plane airport data, local weather (METAR/GRIB), procedures, and the SimBrief flight plan,
4. asks an LLM for a structured briefing, through an HTTP API or a local CLI,
5. speaks the result through Piper TTS inside X-Plane.

Non-goals for v1: replacing ATC (that is `zoal-atc`'s job), real-world operational use, and cloud-only operation. A local LLM through Ollama or LM Studio has to work.

---

## 2. Architecture (target)

```
                 ┌──────────── trigger ────────────┐
[Mic] → WakeWord ┤                                 ├→ StateMachine (orchestrator)
        (later)  └─ X-Plane command / panel button ┘          │
                                                              ▼
            Audio capture → STT (whisper.cpp) → transcript ──┐
                               panel text box (dev) ─────────┤
                                                             ▼
                                                     Intent Parser
                                                             │ IntentResult
                                                             ▼
                                                    Context Assembler
              ┌───────────────┬───────────────┬──────────────┼───────────────┐
              ▼               ▼               ▼              ▼               ▼
         Airport cache   Weather (METAR/  Procedures    SimBrief plan    Aircraft state
         (apt.dat)       TAF/GRIB)        (CIFP/opt.    (JSON fetch)     (datarefs)
                                           Navigraph)
              └───────────────┴───────────────┴──────────────┴───────────────┘
                                                             │ BriefingContext
                                                             ▼
                                                 Prompt builder (templates)
                                                             ▼
                                         LLM Bridge: API mode | CLI mode | fallback
                                                             │ briefing text (sections)
                                                             ▼
                                             TTS (Piper) → playback via XPLMSound
                                                             ▼
                                             Panel shows text + status throughout
```

**Briefing states:**
`IDLE → WAKE_DETECTED → LISTENING → STT_COMPLETE → PARSING → DATA_READY → LLM_ROUTING → BRIEFING_GENERATING → PLAYBACK → IDLE`, plus an `ERROR` exit from every state and a timeout on each state that returns to `IDLE` (to be added, §5.2).

**Threading rule:** the X-Plane main thread (flight loop, draw callbacks) never blocks. Parsing, HTTP, process spawning, STT and TTS run on worker threads. Results come back to the main thread through a queue that the flight loop drains. All XPLM calls happen on the main thread.

---

## 3. What exists today

### 3.1 Working

| Area | Files | Notes |
|---|---|---|
| Build system | `CMakeLists.txt`, `Makefile`, `cmake/*.cmake`, `scripts/ensure-*.sh` | SDK-free core library + tests. The plugin module builds when the SDK is present. FreeType and Inter are fetched by scripts. |
| CI | `.github/workflows/ci.yml` | Tests on macOS, Ubuntu and Windows, and a plugin build on all three. The SDK-compat job is commented out. |
| Plugin entry | `src/xplane/xplane_plugin.cpp` | Start/Stop/Enable/Disable, a per-frame flight loop, and native paths enabled. |
| Logging | `src/xplane/xplane_logger.h` | `XPLMDebugString` in the plugin, stderr in tests. |
| Panel window | `src/xplane/imgui_window.*`, `src/xplane/panel_window.cpp` | Dear ImGui 1.92 rendered through X-Plane panel graphics (not a browser window). Toggle command `zoal_briefing/toggle_panel`, a Plugins menu entry, dragging, resizing, and HiDPI scaling. |
| Panel view | `src/gui/panel_view.cpp`, `panel_session.*`, `panel_theme.*` | Home and Settings tabs, themes (graphite/daylight/night). Settings (wake word, LLM endpoint, model, theme) persist to `zoal-briefing.json` in X-Plane Preferences. |
| METAR field parsing | `src/cache_builder/metar_file_parser.cpp` | Station, wind speed, visibility and weather codes, using regex and substring matching. Covered by a test. |

### 3.2 Stubs and prototypes

| Component | Files | State |
|---|---|---|
| State machine | `state_machine/*` | Holds the state and fires a callback. Transitions are not validated and the timeout is not implemented. Nothing drives it yet. |
| Plugin state | `xplane/plugin_state.*` | A mutex-guarded singleton. The ICAO is hardcoded to `KSFO`, and every getter writes a log line. |
| Intent parser | `intent_parser/*` | Substring keyword matching. `KeywordRouter` is unused. The ICAO regex is broken (§4). |
| LLM API mode | `llm_bridge/api_mode.cpp` | Has a libcurl POST in OpenAI/Ollama format and a response parser, but it is not linked to curl, the streaming method is declared but never defined, and the health check is wrong. |
| LLM CLI mode | `llm_bridge/cli_mode.cpp` | Returns `nullopt`. |
| LLM bridge / fallback | `llm_bridge/llm_bridge.cpp`, `fallback_chain.cpp` | The chain is set up, but it has a lifetime bug and the template fallback always "succeeds" (§4). `llm_bridge_router.cpp` is an empty placeholder. |
| SimBrief client | `simbrief_client/*` | Does not compile and is not in CMake. It scans for `.pln` files, which is not how SimBrief delivers plans. |
| Airport / apt.dat | `data_types/airport.*`, `cache_builder/apt_dat_parser.*` | Two empty parsers. |
| CIFP | `data_types/cifp.*`, `cache_builder/cifp_parser.*` | Two empty parsers. |
| GRIB | `cache_builder/grib_parser.*`, `grib_file_parser.*` | One is empty. The other reads binary GRIB as text lines (a mock). |
| TAF | `cache_builder/taf_parser.*` | Stores the raw text only. |
| Weather summary | `data_types/weather*.cpp`, `cache_builder/weather_summarizer.*` | Hazard flags from METAR codes. There are two competing `wx::summarize` definitions. |
| SID/STAR, Navigraph | `data_types/sid_star.*`, `cache_builder/navigraph_client.*` | An in-memory map, and a client that does nothing. |
| Cache builder | `cache_builder/cache_builder.*` | Starts a thread that logs placeholders. The thread is never joined. |
| Wake word, STT, TTS | `audio_pipeline/*` | Interfaces only. No whisper, Piper or openWakeWord dependency is in the build. |
| Prompts | `prompts/system_prompt.txt`, `prompts/cli_briefing.txt` | Weather-heavy drafts with `{PLACEHOLDER}` fields. Nothing fills them in yet. |
| Config | `briefingmaster.json` | A schema draft. Nothing loads it. |

### 3.3 Tests

`ctest` runs 4 tests, all passing: `metar_parser_tests`, `grib_parser_tests` (mock), `weather_summary_tests` and `intent_parser_tests`. Four more files exist but are not registered or do not compile: `test/apt_dat_parser_test.cpp`, `llm_bridge_test.cpp`, `simbrief_client_test.cpp` and `state_machine_test.cpp`. Tests are split across `test/` and `tests/`. Most tests are placeholders that print PASS.

---

## 4. Known issues in the scaffolding

Fix each of these as part of the component that owns it. Do not fix them in a big-bang cleanup.

**Duplication**
- Two parsers each for apt.dat (`AirportCache::parse_apt_dat` / `AptDatParser`), CIFP (`cifp::CifpParser` / `CifpParserImpl`) and GRIB (`GRIBParser` / `GribFileParser`). Keep one per format.
- `wx::summarize` is defined in both `src/data_types/weather.cpp` (compiled) and `src/data_types/weather_summarizer.cpp` (not compiled), with different divert logic. There is also a third wrapper, `cache_builder/weather_summarizer`.
- Two `panel_window.hpp` files (`include/…/xplane/` and `src/xplane/`) with different `PanelConfig` structs.
- `gui::PluginStatus` duplicates `state::State` and is missing `DATA_READY`.
- Two config files: `briefingmaster.json` (pipeline, never loaded) and `zoal-briefing.json` (panel settings). Merge them into one.

**Correctness**
- `IntentParser` ICAO regex `[EK]A-Z{3}` matches the literal text "A-ZZZ". It should be something like `\b[A-Z]{4}\b`, validated against the airport cache, and must not be limited to E/K prefixes.
- `LlmBridge::generate` captures stack locals (`api`, `cli`, `system_prompt`) by reference in lambdas, then copies the chain into a detached thread. Those references dangle.
- `AUTO` mode probes `ApiMode("")` (an empty endpoint) with a real completion request. This adds a full LLM round-trip to every request.
- The `cached_template` fallback always returns text, so the chain never reports failure, and `voice_prompt` can never run.
- `api_mode.cpp`: the header declares member `health_check`/`parse_response`, but the .cpp defines unrelated static functions. The health check `HEAD`s `<chat-completions-url>/health`. The Anthropic request shape and headers (`x-api-key`, `anthropic-version`) are missing. libcurl is not in `target_link_libraries`.
- `StateMachine::transition_to` calls the user callback while holding the mutex, which deadlocks if the callback transitions again. `is_terminal()` reads without the lock.
- `SimBrief::Client` uses namespace `Fp` (should be `fp`), calls `trigger_weather_refresh` with the wrong arity, and reads `getenv("XPLANE_HOME")` unguarded (null → UB).
- `CacheBuilder` never joins `worker_`, so `std::terminate` runs on destruction.
- `PanelSession` uses POSIX `mkdir`, and its Linux settings path falls back to `"."`.
- The default CLI command `claude --output-format json` has no `-p`, so it would start an interactive session.

---

## 5. Component designs

Each component lists its **scope**, **interface notes** and **definition of done (DoD)**. A component is done when its DoD holds, its tests are registered in CMake, and CI is green.

### 5.1 Foundation: config and plumbing
- One config file, `zoal-briefing.json` in X-Plane `Output/preferences/` (or the plugin folder), loaded at `XPluginStart`. Its sections are `llm_bridge`, `simbrief`, `data`, `audio` and `ui`. The panel reads and writes the same object.
- A main-thread **task queue**: workers post `std::function<void()>`, and `flight_loop` drains it. This is how every async result reaches X-Plane and the panel.
- Register every test and move them all into `test/`.
- **DoD:** settings from the panel and pipeline config round-trip through one file, a worker thread can update the panel through the queue, and all tests run in CI.

### 5.2 State machine and manual trigger
- `StateMachine` becomes the orchestrator. It gets a transition table (illegal transitions are rejected and logged), an `ERROR` state, and per-state timeouts checked from the flight loop (no extra thread). Callbacks fire after the lock is released.
- Triggers: an X-Plane command `zoal_briefing/request_briefing`, and a **text box on the panel** where the pilot types the request. The text box stands in for wake word and STT until §5.9.
- The panel status mirrors `state::State` directly (delete `gui::PluginStatus`).
- **DoD:** typing "departure briefing" on the panel walks the states through to a fake briefing shown on the panel, timeouts return to `IDLE`, and the transition table is unit-tested.

### 5.3 Intent parser
- Output: `IntentResult{type, airport, runway, approach_type, confidence}`.
- Normalize spoken forms before matching. This matters once STT exists: "two eight left" → `28L`, "kilo sierra fox oscar" → `KSFO`, "runway one five right" → `15R`.
- Defaults: no airport → the SimBrief origin (dispatch or departure) or destination (arrival or landing), else the nearest airport to the aircraft. No runway → the active runway from wind, if known.
- Validate against the airport cache. On a mismatch, the plugin asks the pilot to confirm (text on the panel now, speech later).
- Fold `KeywordRouter` into the parser or delete it.
- **DoD:** a table-driven test of 30+ utterances (typed and STT-style) passes.

### 5.4 Airport data (apt.dat)
- Parse X-Plane 12 `apt.dat` from global scenery plus `Custom Scenery` overrides. Read row codes 1/16/17 (airport header), 100 (land runway), 1302 (metadata: ICAO, region, mag var) and 50–56 / 1050–1056 (frequencies).
- Build a lazy, per-airport index (byte offsets per ICAO) at startup on a worker thread. Parse full records on demand. A disk cache in the plugin's cache dir is keyed by the apt.dat mtime and size.
- Provide a nearest-airport lookup from aircraft lat/lon (datarefs).
- **DoD:** startup indexing does not stall the sim, `get("KSFO")` returns its runways with headings and lengths, and the parser is tested on a checked-in apt.dat excerpt.

### 5.5 Weather
- Sources, in order: X-Plane real-weather METAR files (`Output/real weather/`), TAF from the same place when present, then GRIB winds aloft. The exact file names and formats in X-Plane 12 must be checked against a real install before we commit to a parser.
- METAR: extend the existing parser to wind direction and gusts, ceiling (BKN/OVC), altimeter, temp/dew, and a computed flight category (VFR/MVFR/IFR/LIFR).
- GRIB2: replace the text mock with a real decoder. Evaluate eccodes (heavy) against a minimal GRIB2 reader for the few fields we need (UGRD/VGRD/TMP at standard levels). Keep this at the end of the component's scope.
- One summarizer (§4). Its output is a compact, typed `WeatherSummary` that the prompt builder serializes.
- **DoD:** for an airport with real-weather data, the summary has category, wind, ceiling and hazards. Tests use captured METAR/TAF samples.

### 5.6 SimBrief flight plan
- Fetch the latest OFP as JSON with the pilot's SimBrief username or pilot ID (`https://www.simbrief.com/api/xml.fetcher.php?username=<id>&json=1`). This needs no API key. Store the ID in config.
- Triggers: manual refresh from the panel, `XPLM_MSG_PLANE_LOADED`, and optionally a slow poll (≥ 2 min) that changes the cached plan only when `params.request_id` or a content hash changes. Remove the `.pln` file watcher.
- Map the OFP to `fp::FlightPlan`: origin, destination and alternate (with planned runways), route string, SID/STAR, cruise level, fuel blocks, aircraft type, and the OFP's own METAR/TAF where present.
- **DoD:** entering a SimBrief ID on the panel and pressing refresh shows origin, destination and route on the panel. The JSON mapping is tested against a recorded OFP fixture. The HTTP call runs off the main thread.

### 5.7 LLM bridge
**API mode:**
- Supports the OpenAI-compatible `/v1/chat/completions` format (Ollama, LM Studio, OpenAI) and the Anthropic Messages API (`/v1/messages`, `x-api-key`, `anthropic-version`). The provider is chosen by `api.provider`, not guessed from the response.
- libcurl linked properly: vendored or found through `find_package(CURL)` on all three platforms. Connect and total timeouts come from config.
- Streaming (SSE) comes later. It lets TTS start on the first section.

**CLI mode:**
- Spawn the configured command with `posix_spawn`/`CreateProcess`, without a shell. The prompt goes in on stdin, the result is read from stdout, and the process is killed on timeout. Env tokens are passed through only by name, never logged.
- Default for Claude Code: `claude -p --output-format json`, with the result taken from the JSON `result` field. Other CLIs use `output: "text"`.

**Routing and fallback:**
- `mode: api | cli | auto`. In auto mode, use the last-known-good backend. A circuit breaker skips a backend for N minutes after consecutive failures. No probe completions.
- Chain: primary → secondary → **deterministic template** (built from the `BriefingContext` without an LLM, so it is still useful) → spoken/panel error. A template is a real result, but it is labelled as degraded.
- The fallback chain owns its steps by value, and the timeout covers the whole request.

**Output contract:** ask for JSON `{sections: [{title, text}]}`. The parser falls back to treating plain text as one section. Sections map to TTS chunks.

- **DoD:** against a local Ollama, the panel request from §5.2 returns a real briefing. Killing Ollama falls back to CLI, then to the template. Each provider's request and response handling is unit-tested with fixtures (no network in CI).

### 5.8 Prompt builder
- Each briefing type has a template, all built on one shared system prompt:
  - **Dispatch:** route, fuel, alternates, en-route weather, NOTAM placeholder
  - **Departure:** runway, SID, initial altitude, weather, takeoff threats
  - **Arrival:** STAR, expected approach, destination weather, fuel at arrival
  - **Landing:** runway, approach, minimums placeholder, wind/crosswind, go-around
- The current `prompts/*.txt` are weather-only. Rewrite them around this `Phase → Route → Weather → Procedures → Fuel → Threats` structure.
- Context goes in as a compact, labelled block. Missing data is stated as missing so the LLM does not invent it.
- The template files stay on disk in the plugin folder, so users can edit them.
- **DoD:** golden-file tests show each template rendering from a fixed `BriefingContext`.

### 5.9 Audio: TTS, STT, wake word
These come last, in this order. Each one is useful on its own.

1. **TTS (Piper):** link Piper (with onnxruntime and espeak-ng) or run the `piper` binary as a subprocess for v1. Synthesize section by section on a worker thread, and play through X-Plane 12's FMOD-based `XPLMSound` API (`XPLMPlayPCMOnBus` or similar, to be confirmed against SDK 4.4) so it follows sim volume. Provide stop and skip from the panel and a command.
2. **STT (whisper.cpp):** push-to-talk first (hold a command). Capture through miniaudio or PortAudio at 16 kHz mono, with VAD to end utterances after a 5 s cap. Default model `base.en`, with `small.en` as an option.
3. **Wake word (openWakeWord, "Cookie Monster"):** openWakeWord is the chosen engine. Its runtime is Python-first, so in C++ we run its three-stage ONNX pipeline through onnxruntime ourselves: the shared melspectrogram and embedding models, then a custom-trained "Cookie Monster" classifier. We train that classifier with openWakeWord's synthetic-data training notebook and check the `.onnx` model into the models download. Use a confidence threshold and VAD gating. Push-to-talk stays as a manual override, not as a replacement.

`scripts/ensure-models.sh` currently prints links. It should download and verify (checksum) the chosen models instead.

- **DoD (each):** TTS speaks a panel briefing in-sim, push-to-talk produces a transcript on the panel, and the wake word triggers `LISTENING` with fewer than 1 false trigger/hour of cockpit audio.

### 5.10 Procedures (CIFP / Navigraph), optional
- Parse X-Plane's `Custom Data/CIFP/<ICAO>.dat` (or the default `Resources/default data/CIFP`) for SID/STAR/APPCH names, runways and transitions. That is enough to *name* procedures in a briefing. Leg-level detail is not needed for v1.
- Navigraph is out of scope until the rest works. Its licensing and auth need their own review.
- **DoD:** a departure briefing names the SID that SimBrief planned and confirms that it exists for the runway.

---

## 6. Reuse from zoal-atc

zoal-atc is GPL-v3 and this repo is MIT, so **no code is copied**. What we borrow are approaches that are already proven in-sim:

| Pattern | Already applied here | Still to apply |
|---|---|---|
| ImGui through panel graphics, fonts (Inter + FreeType), UI scaling | ✅ `imgui_window`, `panel_window` | n/a |
| SDK/CMake/CI layout, native paths | ✅ | n/a |
| Main-thread task queue, flight-loop polling | n/a | §5.1 |
| Audio capture, mixing and com routing approach | n/a | §5.9 |
| LLM/CLI process handling | n/a | §5.7 |

If we ever want actual code from zoal-atc, the options are to relicense that code (the same author can) or to keep it behind a process boundary. Decide this before §5.9.

---

## 7. Cross-cutting requirements

- **Main thread:** no blocking work in flight-loop or draw callbacks. Budget < 0.5 ms per frame for the plugin.
- **Secrets:** API keys come from env vars or the OS keychain, and are never written to logs or to the config in plain text where avoidable. Prompts are not logged by default (there is a debug flag).
- **Logging:** use `xplane_debug_log` with a component prefix. No logging from per-frame getters (remove the logs in `PluginState`).
- **Latency targets (once audio exists):** < 2 s from end of speech to the first spoken word in API mode with streaming, and < 6 s for a full briefing.
- **Offline:** with no LLM reachable, the template briefing still works from local data.
- **Testing:** the core stays SDK-free and testable on all three platforms. Network and process calls go behind interfaces with fakes in tests.

---

## 8. Roadmap: one component at a time

| # | Component | Section | Depends on | Milestone |
|---|---|---|---|---|
| 0 | Scaffolding: build, CI, plugin, panel | n/a | n/a | ✅ **Done.** Plugin loads, panel works. |
| 1 | Foundation: config, task queue, tests | §5.1 | 0 | One config file, all tests in CI |
| 2 | State machine + typed request on panel | §5.2 | 1 | End-to-end fake briefing from text |
| 3 | Intent parser | §5.3 | 2 | Typed request → `IntentResult` |
| 4 | Airport data (apt.dat) | §5.4 | 1 | Runways and nearest airport |
| 5 | SimBrief | §5.6 | 1 | OFP on the panel |
| 6 | Weather (METAR/TAF; GRIB last) | §5.5 | 4 | Weather summary per airport |
| 7 | Prompt builder + template fallback | §5.8 | 3–6 | Deterministic briefing with no LLM |
| 8 | LLM bridge: API mode | §5.7 | 7 | Real briefing from Ollama |
| 9 | LLM bridge: CLI mode + routing | §5.7 | 8 | Fallback chain verified |
| 10 | TTS (Piper) + XPLMSound playback | §5.9 | 8 | Briefing spoken in-sim |
| 11 | STT (whisper.cpp), push-to-talk | §5.9 | 10 | Spoken request → transcript |
| 12 | Wake word "Cookie Monster" | §5.9 | 11 | Hands-free activation |
| 13 | Procedures (CIFP), optional | §5.10 | 4 | SID/STAR named and validated |

The order keeps the plugin usable after every step: from step 7 onward, every later component improves a briefing that already works end to end.

---

## 9. Open questions

1. Config location: X-Plane `Output/preferences/` (per-install) or the plugin folder (travels with the plugin)?
2. Piper: link it in-process (bigger binary, onnxruntime on three platforms) or run it as a subprocess for v1?
3. GRIB: is eccodes acceptable as a dependency, or do we write a minimal GRIB2 reader?
4. Wake word: onnxruntime will be needed for openWakeWord (and Piper, if Piper runs in-process). Do we vendor it, or use the prebuilt releases per platform?
5. zoal-atc code reuse: relicense specific modules, or stay pattern-only (§6)?
