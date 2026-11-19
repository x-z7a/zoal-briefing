# BriefingMaster — Voice-Driven Aviation Briefing System for X-Plane

**BriefingMaster** is a voice-driven dispatch/departure/arrival/landing briefing system that activates via the wake word **"hey jarvis"**, captures pilot speech, transcribes it with Whisper, routes intent via keyword parsing, aggregates context from cached airport/SID/STAR/weather data and SimBrief flight plans, generates a structured briefing through a dual-path LLM bridge (API or CLI), and delivers it audibly via Piper TTS into the X-Plane com system.

## Architecture Overview

```
[Microphone] → WakeWord Engine → Audio Buffer → STT (Whisper)
                                                 ↓
                                        Intent Parser (Keywords/Regex)
                                                 ↓
              ┌──────────────────────────────────┼──────────────────────────────────┐
              ↓                                  ↓                                  ↓
        SimBrief Trigger                 Startup Cache Builder                State Machine
              ↓                                  ↓                                  ↓
       Flight Plan Data               APT.DAT/CIFP Parser                  Thread-safe context
                                     GRIB Weather Parser               + timeout/recovery logic
                                     Navigraph (opt) ──────────────┐
                                                               ↓
                                              LLM Bridge Router
                                    ┌───────────┴───────────┐
                                    ↓                         ↓
                              API Mode (OpenAI/Anthropic/    CLI Mode (Claude/Copilot/
                              Ollama/LM Studio)             Codex/Custom Prompts)
                                    ↓                         ↓
                              Structured Briefing Text      Structured Briefing Text
                                    └───────────┬───────────┘
                                              ↓
                                          TTS (Piper) → Audio Mixer → X-Plane Output
```

**State Machine:** `IDLE → WAKE_DETECTED → LISTENING → STT_COMPLETE → PARSING → DATA_READY → LLM_ROUTING → BRIEFING_GENERATING → PLAYBACK → IDLE`

## Module Reference

| Module | Header | Source | Purpose |
|--------|--------|--------|---------|
| **state_machine** | `zoal_briefing/state_machine/state_machine.hpp` | `src/state_machine/state_machine.cpp` | State transitions & callbacks for the full briefing workflow |
| **cache_builder** | `zoal_briefing/cache_builder/apt_dat_parser.hpp` | `src/cache_builder/apt_dat_parser.cpp` | Parse APT.DAT into airport/runway data structures |
| **cache_builder** | `zoal_briefing/cache_builder/cifp_parser.hpp` | `src/cache_builder/cifp_parser.cpp` | Parse CIFP terminal navigation data |
| **cache_builder** | `zoal_briefing/cache_builder/grib_parser.hpp` | `src/cache_builder/grib_file_parser.cpp` | Parse GRIB2 weather files into wind profiles |
| **cache_builder** | `zoal_briefing/cache_builder/navigraph_client.hpp` | `src/cache_builder/navigraph_client.cpp` | Fetch SID/STAR from Navigraph AIP API |
| **cache_builder** | `zoal_briefing/cache_builder/cache_builder.hpp` | `src/cache_builder/cache_builder.cpp` | Orchestrates startup caching of all data sources |
| **intent_parser** | `zoal_briefing/intent_parser/intent_parser.hpp` | `src/intent_parser/intent_parser.cpp` | Parse pilot utterance into briefing type + params |
| **intent_parser** | `zoal_briefing/intent_parser/keyword_router.hpp` | `src/intent_parser/keyword_router.cpp` | Keyword scoring for robust intent detection |
| **llm_bridge** | `zoal_briefing/llm_bridge/llm_bridge.hpp` | `src/llm_bridge/llm_bridge_router.cpp` | Dual-mode LLM router (API + CLI with fallback) |
| **llm_bridge** | `zoal_briefing/llm_bridge/api_mode.hpp` | `src/llm_bridge/api_mode.cpp` | HTTP/REST to OpenAI/Anthropic/Ollama/LM Studio |
| **llm_bridge** | `zoal_briefing/llm_bridge/cli_mode.hpp` | `src/llm_bridge/cli_mode.cpp` | Process spawn for Claude/Copilot/Codex CLI |
| **llm_bridge** | `zoal_briefing/llm_bridge/fallback_chain.hpp` | `src/llm_bridge/fallback_chain.cpp` | API → CLI → cached template → voice prompt chain |
| **simbrief** | `zoal_briefing/simbrief/client.hpp` | `src/simbrief/client.cpp` | Monitor SimBrief state changes, fetch flight plan data |
| **audio** | `zoal_briefing/audio/wake/openwakeword.hpp` | `src/audio/wake/openwakeword.cpp` | openwakeword detection for "hey jarvis" |
| **audio** | `zoal_briefing/audio/stt/whisper_stt.hpp` | `src/audio/stt/whisper_stt.cpp` | whisper.cpp transcription (16-bit 16kHz PCM) |
| **audio** | `zoal_briefing/audio/tts/piper_tts.hpp` | `src/audio/tts/piper_tts.cpp` | Piper TTS → X-Plane com audio mixer |
| **xplane** | `zoal_briefing/xplane/plugin_state.hpp` | `src/xplane/plugin_state.cpp` | Plugin-wide singleton state (current ICAO, active briefing) |

## Data Types

| Type | Header | Purpose |
|------|--------|---------|
| `AirportInfo` / `RunwayData` | `zoal_briefing/data_types/airport.hpp` | Airport/runway data from APT.DAT/CIFP |

| `FlightPlanInfo` | `zoal_briefing/data_types/flight_plan.hpp` | Flight plan from SimBrief |
| `SIDSTARData`    | `zoal_briefing/data_types/sid_star.hpp`    | Terminal nav data (SIDs/STARs) |

## Build & Install

### Prerequisites

- CMake ≥ 3.24, C++17 compiler (AppleClang / GCC / Clang / MSVC)
- X-Plane SDK (auto-downloaded on first build when `ZoalBriefing_BUILD_PLUGIN=ON`)
- Optional: zoal-atc reference repo for header lookup (`-DZoalBriefing_ATC_DIR=...`)

### Makefile Development Workflow

```bash
make help        # Show all targets and descriptions
make build-tests # Build core library + unit tests (no SDK needed)
make test        # Run unit tests only
make build-plugin  # Build X-Plane plugin module (.xpl)
make install-plugin # Install .xpl to X-Plane Developer/Plugins folder
make plugin      # Build + install in one command
make sdk-update    # Fetch latest X-Plane SDK upstream changes
make sdk-reset     # Re-clone SDK from scratch
make clean         # Remove build artifacts (keeps sdk/)
make distclean     # Full reset (including SDK re-clone)
```

### CMake Configuration

```bash
# Tests only (no SDK required)
cmake -B build-test   -DZoalBriefing_BUILD_TESTS=ON   -DZoalBriefing_BUILD_PLUGIN=OFF

# Plugin + tests (requires X-Plane SDK)
cmake -B build-plugin   -DZoalBriefing_BUILD_TESTS=ON   -DZoalBriefing_BUILD_PLUGIN=ON   -DZoalBriefing_XPLANE_SDK=/path/to/xpsdk

# Or let CMake auto-download the SDK:
cmake -B build-plugin   -DZoalBriefing_BUILD_TESTS=ON   -DZoalBriefing_BUILD_PLUGIN=ON
```

### Build Output

| Target          | Output Path                            |
|-----------------|----------------------------------------|
| Core library    | `build-test/libzoal_briefing_core.a`   |
| Unit tests      | `build-test/*_tests` (executable)      |
| X-Plane plugin  | `build-plugin/zoal_briefing.xpl`       |

### Local Plugin Testing (`make install-plugin`)

```bash
# macOS — installs to X-Plane Developer/Plugins folder
make install-plugin

# Custom X-Plane plugins directory
XPLANE_DEV_PLUGINS=/path/to/X-Plane/Resources/plugins make install-plugin
```

The plugin copies `zoal_briefing.xpl` into:
- **macOS:** `~/Library/Application Support/X-Plane/Developer/Plugins/`
- **Windows:** `C:/X-Plane/Developer/Plugins/`
- **Linux:** `/opt/X-Plane/Developer/Plugins/`

## CI Integration

This project uses a three-job CI pipeline:

### Job 1: Tests (macOS, Ubuntu, Windows)
Builds the core library and runs unit tests — **no X-Plane SDK required**.

### Job 2: Plugin Build (all platforms)
Downloads X-Plane SDK from the official X-Plane CDN via CMake's `file(DOWNLOAD)` — **no git clone, no auth issues**. Builds `.xpl` for all three platforms.

### Job 3: SDK Compatibility Check (weekly)
Clones the latest X-Plane SDK and verifies the project still builds against it. Detects upstream breakages early.

### How `scripts/ensure-sdk.sh` Works

For local development, this script ensures the `sdk/` directory always has the latest X-Plane SDK:

1. Clones (or re-clones) `xpsdk.git` into `sdk/`
2. Fetches HEAD from upstream (`origin/master` → auto-fallback to `origin/main`)
3. Reports differences and syncs automatically

```bash
./scripts/ensure-sdk.sh                    # Default repo, local sdk/ dir
./scripts/ensure-sdk.sh https://.../custom  # Override URL
./scripts/ensure-sdk.sh /my/local/sdk       # Use existing directory
```

### Weekly SDK Compat Job

The `.github/workflows/ci.yml` includes a scheduled job (weekly, Monday 6am UTC) that:
- Clones the latest X-Plane SDK fresh from GitHub
- Reports new commits in `#sdk-updates` or GitHub Issues
- Attempts to build tests against the latest SDK
- Summarizes results in the workflow summary

## Next Steps / TODO

1. **Audit zoal-atc license** — This project uses MIT license (independent of GPL-v3 zoal-atc). For zoal-atc code reuse, extract only the audio pipeline patterns and create modular interfaces.
2. **Implement real LLM bridge** — Currently placeholder HTTP/CLI stubs that need curl/httplib integration.
3. **Wire up state machine to dataflow** — Connect the `xplane/plugin.cpp` flight loop to actual cache builder completion triggers and SimBrief load events.
4. **Add X-Plane com audio playback** — `play_audio_chunk()` needs `XPLMPlaySoundFile2` or buffer callback to output TTS through selected COM radio.
5. **SimBrief integration** — Implement the SimBrief API client to fetch flight plan data and monitor load state changes.
6. **Config file loading** — Read `briefingmaster.json` from the plugin directory on startup for LLM config.

## License

MIT — see [LICENSE](LICENSE) for details.
