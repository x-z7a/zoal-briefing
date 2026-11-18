#!/bin/bash
# Download required model files for BriefingMaster.
# Usage: ./scripts/ensure-models.sh [models_dir]

MODELS_DIR="${1:-$(pwd)/model}"

mkdir -p "$MODELS_DIR/whisper"
mkdir -p "$MODELS_DIR/openwakeword"  
mkdir -p "$MODELS_DIR/piper"

echo "Model files are required for production use."
echo ""
echo "Whisper (STT):"
echo "  Download ggml-small.en.bin from:"
echo "  https://huggingface.co/ggerganov/whisper.cpp/blob/main/ggml-small-en.bin"
echo "  Place at: $MODELS_DIR/whisper/ggml-small.en-q5_1.bin"
echo ""
echo "OpenWakeWord (wake word detection):"
echo "  Build openwakeword models:"
echo "  https://github.com/k2-fsa/openwakeword"
echo ""
echo "Piper (TTS):"
echo "  Download piper model from:"
echo "  https://github.com/rhasspy/piper/releases"
echo "  Place at: $MODELS_DIR/piper/"

