# Training the "cookie monster" wake word

The plugin listens with [openWakeWord](https://github.com/dscripka/openWakeWord).
openWakeWord publishes models only for a few phrases (`hey_jarvis`, `alexa`,
`hey_mycroft`, `hey_rhasspy`), so **"cookie monster" needs a model of its own**.
Until one exists, the plugin falls back to `hey_jarvis_v0.1.onnx` and logs:

```
[zoal-briefing] WARN [audio] no model for wake word "cookie monster" (expected openwakeword/cookie_monster.onnx); using "hey jarvis" until it is trained
```

Everything else in the pipeline is already the real thing. Only the classifier
file changes.

## How the plugin finds the model

The wake word is the `wake_word` setting (default `cookie monster`, editable on
the panel's Settings tab). The plugin turns it into a file name and looks in
`models/openwakeword/` for:

1. `cookie_monster.onnx`
2. `cookie_monster_v0.1.onnx`
3. otherwise the fallback, `audio.fallback_wake_model` (`hey_jarvis_v0.1.onnx`)

The shared front end (`melspectrogram.onnx`, `embedding_model.onnx`) is the
same for every wake word. `scripts/ensure-models.sh` downloads it.

## Train it

openWakeWord trains a small classifier on top of its shared embedding, using
synthetic speech: Piper voices say the phrase thousands of times with varied
speakers, speeds and room noise. Its repository has a training notebook that
runs on Google Colab (a free GPU runtime is enough, and it takes about an hour).

1. Open the openWakeWord repo's training notebook in Colab
   (`notebooks/automatic_model_training.ipynb`, linked from its README under
   "Training New Models").
2. Set the target phrase to `cookie monster`. Spell it the way a speech engine
   would say it. If the samples it plays back sound wrong, try `cookie monstar`.
3. Keep the defaults for the first model. The notebook's own false-positive
   validation reports how often it fires on hours of unrelated speech.
4. Download the exported `.onnx` (not the `.tflite`).

## Install it

```bash
cp cookie_monster.onnx .cache/models/openwakeword/cookie_monster.onnx
make voice-check
```

The self-test runs **every** classifier in `openwakeword/`: Piper speaks the
phrase (taken from the file name), and the model must fire on it and stay quiet
on an unrelated sentence. You should see:

```
PASS  wake "cookie monster" fires on the spoken phrase (peak 0.97, ...)
PASS  wake "cookie monster" stays quiet on other speech (peak 0.00)
```

Then `make install` copies the models into the plugin folder
(`<X-Plane>/Resources/plugins/zoal-briefing/models/`). On the next start the
plugin logs:

```
[zoal-briefing] [audio] wake word "cookie monster" loaded (cookie_monster.onnx, threshold 0.50, ...)
```

## Tuning

In `zoal-briefing.json` (X-Plane's `Output/preferences/` folder):

```json
{
  "wake_word": "cookie monster",
  "audio": { "wake_threshold": 0.5 }
}
```

- **It misses you:** lower `wake_threshold` (0.3–0.4), or retrain with more
  samples.
- **It fires on cockpit chatter or the sim's own sounds:** raise it (0.6–0.7).
  Every detection is logged with its score (`heard "cookie monster" (score
  0.86)`), so Log.txt shows how close the false ones were.

`make voice-live` runs the same loop on your Mac's microphone and speakers
outside X-Plane, which is the quickest way to tune.
