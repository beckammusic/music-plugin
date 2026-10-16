# Architecture and engineering decisions

## Real-time processing

`PluginProcessor` owns an APVTS and caches atomic parameter pointers through `ParameterReader`. One bounded snapshot is read per host block. Audio is processed as float, with denormals disabled. `Engine` chunks unexpectedly large host blocks into the storage capacity allocated by prepareToPlay. Zero-length blocks return safely. Mono is duplicated internally for shared stereo DSP, then only the mono result is returned. There are no callback files, GUI calls, blocking locks, or heap reconstruction.

Continuous DSP controls use sample-rate-aware one-pole interpolation; module and master bypass fade. All modes stay initialized. State/preset serialization can allocate and uses a dedicated lock for comparison-slot access, exclusively outside audio processing. GUI parameter changes pass through host-aware JUCE attachments. Block-level parameter snapshots follow APVTS semantics; rapid multi-parameter preset changes are smoothed but are not a host-wide atomic transaction.

## Oversampling and latency

Color and Clip have separate JUCE equiripple halfband FIR banks at Off/2x/4x. The rest of the chain stays at base rate. All banks are prepared on the lifecycle thread and continue processing; mode weights fade at base rate. This avoids quality changes allocating, replacing resources, or calling setLatencySamples in processBlock.

Pinned JUCE's unpadded FIR round-trip latencies are 0,49,59.5 base samples. JUCE's optional integer-latency Thiran allpass is disabled to preserve FIR linear phase. At4x, two high-rate samples are delayed between shaping and downsampling, completing the half-sample base delay exactly. Integer delays then align every bank to60 samples. Two nonlinear stages yield120 samples total; the global original-input path is delayed120. Independent bypass keeps each stage's delay60, and host bypass continues all states while blending to the aligned original signal.

FIR magnitude response naturally changes near Nyquist; group-delay alignment does not mean every filtered signal is sample-identical to unfiltered dry audio. Saturation's own blend occurs at the high rate. Zero Color drive/mix disables the stage's colored contribution. Master/global dry do not include Input or Output gain. Compressor blending is local and zero latency.

A fixed ring buffer is used for these short deterministic delays. Memory is reset and capacity prepared only in prepareToPlay. Filter sample-rate changes are host lifecycle operations, outside playback processing.

## Nonlinear behavior

Tape combines a symmetric tanh curve with a small low-passed contribution. Tube uses a biased tanh curve with bias subtraction to emphasize even harmonics. Color blends stronger tanh and bounded sine shaping. Input drive maps deterministically to0–24dB and compensation to the inverse square root of gain, with fixed mode trims. A12Hz blocker removes nonlinear DC. This is a creative analog-inspired processor, not a component-level analog circuit model. At zero drive the saturation DSP is unity.

Clip drive maps0–24dB; hard clipping clamps normalized samples at±1. Soft clipping is linear below0.8 ceiling and transitions through a continuous tanh knee. Both symmetric curves avoid adding DC to symmetric input; existing DC is not an advertised removal feature of Clip. A base-rate clamp after FIR reconstruction enforces the steady sample ceiling despite filter ringing. The selected ceiling is smoothed for automation. No intersample/true-peak detector is implemented. Global dry, Output gain, or module bypass fades may exceed the Clip ceiling by design.

Activity meters represent selected/crossfading paths rather than unselected quality banks. Peak meters have time-based release, use atomics, and the native UI polls at30Hz.

## State and identity

APVTS ID `GUMMY_STATE`, schemaVersion1. Parameter child IDs are fixed and validated for presence, uniqueness, finite value, and range before restore. Factory presets start from defaults then apply documented overrides. User presets are actual XML parameter snapshots. A/B slots are per instance and included in DAW state. Unknown schema versions are rejected rather than partially restoring incompatible data.

ManufacturerGmAu/pluginGmmy/bundlecom.gummyaudio.gummy are stable. All controls and choices are documented in PARAMETERS.md. Source-defined and saved factory snapshots should remain in sync.

## Validation limits

Synthetic tests establish numerical behavior and selected timing, response, DC, alias, stability, state, layout, and allocation properties. They do not establish subjective musical quality on every source, guaranteed inaudibility of any rapid automation, all possible host threading behavior, or actual Apple host compatibility. Allocation instrumentation counts C++new operations inside processBlock; it complements code review and does not instrument arbitrary operating-system allocation APIs.

Professional listening with representative vocals, drums, bass, buses, and masters, third-party validators, and actual Logic projects on both Apple architectures are release requirements recorded as pending when unavailable.
