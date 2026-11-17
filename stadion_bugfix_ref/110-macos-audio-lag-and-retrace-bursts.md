# macOS: sound ~90 ms behind the picture; overdue retraces burst frames into the audio queue

- **Category:** audio
- **Zero status:** already ported (`src/port/audio_out.cpp` is byte-identical to stadion's; pacing code present)
- **stadion:** [PR #110](https://github.com/team-hippodrome/stadion/pull/110), commit(s) `543ed0b` (merge `31368be`)
- **stadion files:** `src/port/audio_out.cpp`, `src/port/os_devices.cpp`, `src/port/shader_warmup.txt`, `src/port/vulkan_pipeline_warmup.txt`

## Symptom
Intro audio about 91 ms behind the picture on macOS (ares: 37 ms).

## Root cause
SDL's default Core Audio period exceeded a retrace and the queue target was 70 ms; the device started pulling late
so the opening cue sat behind a growing queue; after a stall the retrace clock ran several overdue retraces
back-to-back, pushing a burst of game audio into the queue.

## Fix in stadion
On Apple: `SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES=256` for the stream, queue target 45 ms, the device is started and
primed (wait for the first get-callback) before game samples; after an underrun it refills to target before
resuming; the retrace clock allows at most one overdue period before resyncing (later raised to a 100 ms make-up by
#118, see 118).

## Zero / plethron
`diff -q` of `src/port/audio_out.cpp` against stadion is clean: `kDefaultQueueTarget = 0.045` (Apple) at :51 and the
sample-frames hint at :120-121. Zero's audio goes through the same `audio_out.cpp`.

## How to detect
stadion's `tools/av_sync.py` (picture-to-sound lag against a video); `PORT_TRACE_AUDIO_OUT=1` prints device start
and rebuffer events.
