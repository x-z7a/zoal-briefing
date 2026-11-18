# BriefingMaster — Voice-Driven Aviation Briefing System for X-Plane

**BriefingMaster** is a voice-driven dispatch/departure/arrival/landing briefing system that activates via the wake word **"Cookie Monster"**, captures pilot speech, transcribes it with Whisper, routes intent via keyword parsing, aggregates context from cached airport/SID/STAR/weather data and SimBrief flight plans, generates a structured briefing through a dual-path LLM bridge (API or CLI), and delivers it audibly via Piper TTS into the X-Plane com system.

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
| **state_machine** | `master/state_machine/state_machine.hpp` | `src/state_machine/state_machine.cpp` | State transitions & callbacks for the full briefing workflow |
| **cache_builder** | `master/cache_builder/apt_dat_parser.hpp` | `src/cache_builder/apt_dat_parser.cpp` | Parse APT.DAT into airport/runway data structures |
| **cache_builder** | `master/cache_builder/cifp_parser.hpp` | `src/cache_builder/cifp_parser.cpp` | Parse CIFP terminal navigation data |
| **cache_builder** | `master/cache_builder/grib_parser.hpp` | `src/cache_builder/grib_parser.cpp` | Parse GRIB2 weather files into wind profiles |
| **cache_builder** | `master/cache_builder/navigraph_client.hpp` | `src/cache_builder/navigraph_client.cpp` | Fetch SID/STAR from Navigraph AIP API |
| **cache_builder** | `master/cache_builder/cache_builder.hpp` | `src/cache_builder/cache_builder.cpp` | Orchestrates startup caching of all data sources |
| **intent_parser** | `master/intent_parser/intent_parser.hpp` | `src/intent_parser/intent_parser.cpp` | Parse pilot utterance into briefing type + params |
| **intent_parser** | `master/intent_parser/keyword_router.hpp` | `src/intent_parser/keyword_router.cpp` | Keyword scoring for robust intent detection |
| **llm_bridge** | `master/llm_bridge/llm_bridge.hpp` | `src/llm_bridge/llm_bridge.cpp` | Dual-mode LLM router (API + CLI with fallback) |
| **llm_bridge** | `master/llm_bridge/api_mode.hpp` | `src/llm_bridge/api_mode.cpp` | HTTP/REST to OpenAI/Anthropic/Ollama/LM Studio |
| **llm_bridge** | `master/llm_bridge/cli_mode.hpp` | `src/llm_bridge/cli_mode.cpp` | Process spawn for Claude/Copilot/Codex CLI |
| **llm_bridge** | `master/llm_bridge/fallback_chain.hpp` | `src/llm_bridge/fallback_chain.cpp` | API → CLI → cached template → voice prompt chain |
| **simbrief_client** | `master/simbrief_client/client.hpp` | `src/simbrief_client/client.cpp` | Monitor SimBrief state changes, fetch flight plan data |
| **audio_pipeline** | `master/audio_pipeline/wake_word_engine.hpp` | `src/audio_pipeline/wake_word_engine.cpp` | openwakeword detection for "Cookie Monster" |
| **audio_pipeline** | `master/audio_pipeline/stt_transcriber.hpp` | `src/audio_pipeline/stt_transcriber.cpp` | whisper.cpp transcription (16-bit 16kHz PCM) |
| **audio_pipeline** | `master/audio_pipeline/tts_speaker.hpp` | `src/audio_pipeline/tts_speaker.cpp` | Piper TTS → X-Plane com audio mixer |
| **xplane** | `master/xplane/plugin_state.hpp` | `src/xplane/plugin_state.cpp` | Plugin-wide singleton state (current ICAO, active briefing) |

## Data Types

| Type | Header | Purpose |
|------|--------|---------|
| `AirportInfo` / `RunwayData` | `master/data_types/airport.hpp` | Airport/runway data from APT.DAT/CIFP |
| `TerminalNavData` | `master/data_types/cifp.hpp` | CIFP terminal nav records (TOFL, NAVDAT) |
| `WindProfile` / `METARData` | `master/data_types/weather.hpp` | Weather data: wind aloft + current METAR |
| `FlightPlan` / `Waypoint` / `RouteLeg` | `master/data_types/flight_plan.hpp` | SimBrief flight plan structures |
| `SIDProcedure` / `STARProcedure` | `master/data_types/sid_star.hpp` | Navigraph SID/STAR procedures |

## Configuration Schema (`briefingmaster.json`)

```json
{
  "llm_bridge": {
    "mode": "auto",
    "api": {
      "provider": "ollama",
      "endpoint": "http://localhost:11434/v1/chat/completions",
      "model": "qwen3.6:35b-a3b",
      "api_key": ""
    },
    "cli": {
      "provider": "claude",
      "command": "claude --output-format json",
      "env_tokens": ["ANTHROPIC_API_KEY"],
      "prompt_template_path": "./prompts/cli_briefing.txt"
    }
  }
}
```

## Building

```bash
# Create build directory
mkdir build-plugin && cd build-plugin

# Configure (adjust XPLANE_SDK path to your SDK location)
cmake .. -DZOAL_BRIEFING_BUILD_PLUGIN=ON \
         -DATC_XPLANE_SDK=/path/to/xplane-sdk

# Build the .xpl plugin
make

# The .xpl file drops into: build-plugin/zoal_briefing.xpl
```

For tests only (no X-Plane SDK required):
```bash
mkdir build-test && cd build-test
cmake .. -DZOAL_BRIEFING_BUILD_TESTS=ON
make
ctest
```

## Dependencies

- **X-Plane SDK**: C++17, `xplm.h`, `xplm_dataref.h`
- **Audio**: PortAudio (or system native), openwakeword, whisper.cpp, piper-cpp
- **Networking**: nlohmann/json, libcurl/httplib
- **LLM Bridges**: OpenAI SDK, Ollama CLI, Claude CLI (optional)

## Next Steps / TODO

1. **Audit zoal-atc license** → This project uses MIT license (independent of GPL-v3 zoal-atc). For zoal-atc code reuse, extract only the audio pipeline patterns and create modular interfaces.
2. **Implement real LLM bridge** — Currently placeholder HTTP/CLI stubs that need curl/httplib integration.
3. **Wire up state machine to dataflow** — Connect the `xplane_plugin.cpp` flight loop to actual cache builder completion triggers and SimBrief load events.
4. **Add X-Plane com audio playback** — `play_audio_chunk()` needs `XPLMPlaySoundFile2` or buffer callback to output TTS through selected COM radio.
5. **SimBrief integration** — Implement the SimBrief API client to fetch flight plan data and monitor load state changes.
6. **Config file loading** — Read `briefingmaster.json` from the plugin directory on startup for LLM config.

## License

MIT — see [LICENSE](LICENSE) for details.

## CI Integration

This project includes a CI-ready `.github/workflows/ci.yml` that uses the
`ensure-sdk.sh` helper to **always build against the latest X-Plane SDK**.

### How `scripts/ensure-sdk.sh` works

1. Checks if the given path exists and contains valid SDK headers
2. If it's a git repository, fetches HEAD from upstream (default: `origin/main`)
3. Compares local HEAD with upstream — reports differences clearly
4. In CI (detected automatically): **auto-pulls silently** before the build
5. In interactive mode: prompts the user before pulling

### Usage in CI

```yaml
- name: Clone X-Plane SDK
  run: git clone https://github.com/LaminarResearch/X-Plane-SDK.git ~/git/xplane-sdk

- name: Ensure SDK latest & build
  run: |
    ./scripts/ensure-sdk.sh ~/git/xplane-sdk
    mkdir build && cd build
    cmake .. -DATC_XPLANE_SDK=$HOME/git/xplane-sdk
    make
```

### Flags & Environment

| Argument / Env Var       | Purpose                                    |
|--------------------------|--------------------------------------------|
| `--no-pull`              | Check only — never auto-pull               |
| `$XPLANE_SDK_REMOTE`     | Override the git remote to check           |
| `$CI_FORCE_CHECK_ONLY=1` | Force check-only mode (reporting only)      |

The script detects CI automatically via common environment variables (`CI`,
`GITHUB_ACTIONS`, `GITLAB_CI`, etc.) and non-interactive mode (no tty).

### Weekly SDK Compat Job

The `.github/workflows/ci.yml` includes a scheduled job (weekly, Monday 6am UTC)
that:
- Clones the latest X-Plane SDK fresh
- Reports any new commits in `#sdk-updates` or GitHub Issues
- Attempts to build tests against the latest SDK
- Summarizes results in the PR/commit summary
