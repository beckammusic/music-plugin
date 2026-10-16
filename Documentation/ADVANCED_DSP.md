# Advanced DSP implementation

All three processors are original, standard-library-only C++17 implementations in `Source/DSP`. Their common interface is `prepare(double sampleRate)`, `reset()`, and `process(float& left, float& right, const Settings&) noexcept`. Call `prepare` before audio starts or when the sample rate changes. Their algorithmic latency is zero: each includes an undelayed dry path and none uses lookahead. Modulation delay is an audible effect, not an additional delay applied to the whole signal. The plugin chain controls module bypass and smooths its bypass transitions.

## Musical EQ

`MusicalEQ` implements four cascaded minimum-phase biquads with broad, fixed curves:

| Macro | Internal response | Full-scale setting |
| --- | --- | --- |
| BODY | 150 Hz low shelf plus broad 280 Hz bell, Q 0.55 | Shelf ±8.55 dB and bell ±2.7 dB; overlapping composite stays approximately within ±9 dB |
| PRESENCE | 2.8 kHz broad bell, Q 0.65 | ±9 dB |
| AIR | 9 kHz high shelf | ±9 dB |

Shelves use a gentle slope of 0.75. Controls have 30 ms exponential smoothing; coefficient targets update every 16 samples and individual coefficients interpolate between updates. Internal states use double precision, while the public audio interface remains float. Center frequencies cap at 42% of sample rate for defensive operation outside the supported 44.1–192 kHz range. Zero settings produce unity response. This is broad tone shaping; it provides no surgical bands or independently adjustable frequency/Q.

## Resonance suppression

`ResonanceSuppressor` uses a fixed bank of 24 parallel, constant-Q bandpass detectors spanning 160 Hz–16 kHz. The upper frequency caps below Nyquist. Each band's energy is compared against the geometric mean of two neighboring bands, two positions away where possible to include peaks falling between centers. The same linked attenuation controls both channels, preserving stereo correspondence. Attenuation subtracts a smoothly controlled portion of a bandpass output from the undelayed input; this is equivalent to a dynamic notch at that band's center rather than a global treble reduction.

The detector uses 8 ms attack and 100 ms release envelopes and refreshes spectral analysis every 32 samples. A frequency-dependent threshold favors the midrange and highs; low bands receive less processing. The amount sets threshold and a maximum nominal attenuation of 9 dB per band. Reduction attacks over 20 ms and normally recovers over 180 ms. Setting amount to zero releases active attenuation over 20 ms, then restores an exact dry path; a fresh zero setting is already exact dry. A spectral-flatness confidence gate reduces treatment of isolated steady tones, and a low-level gate avoids pursuing the noise floor. `activity()` reports the strongest band's positive nominal attenuation in dB; it is not broadband gain reduction.

A finite, fixed-frequency bank cannot distinguish unwanted resonances from every desirable musical harmonic. Detection is strongest near band centers, broad plateaus may remain, and confidence intentionally falls for sparse tonal sources. The first/last detector bands serve as neighbors rather than attenuation bands. The maximum applies per band; overlapping responses and phase interactions mean actual measured attenuation can differ. This low-latency implementation is not an FFT spectral editor and does not claim the behavior of any proprietary processor.

## Modulation

`Modulation` allocates three stereo circular buffers in `prepare` only. Chorus, flanger, and doubler run continuously, including when another mode is selected. The selector crossfades their outputs with a 30 ms exponential curve, so a new selection never starts with an empty buffer. Amount smoothing is 35 ms. All delay reads use bounded four-point cubic Hermite interpolation. LFO phases evolve continuously; mode changes never reset them.

| Mode | Delay and modulation | Maximum wet contribution |
| --- | --- | --- |
| Chorus | 13/14 ms nominal, up to ±3.5 ms; 0.18–0.40 Hz LFO; offset stereo phase and gentle crossfeed | 42% |
| Flanger | Approximately 0.7–5.5 ms swept delay; 0.09–0.20 Hz LFO; stereo phase offset | 45% |
| Doubler | Two voices per channel, nominal 19–30 ms; independent 0.41/0.67 Hz modulation, up to ±2.5 ms | 40% |

Flanger feedback reaches 0.58 and includes a smooth bounded feedback limiter. The margin accounts for cubic-interpolator overshoot; maximum feedback remains contractive. Amount controls both depth/feedback character and wet contribution. Fresh zero-amount operation returns the input exactly, and changes to zero fade smoothly. Linear dry/wet blending avoids excess summed gain at DC. Mono summing remains usable but, as with physical chorus and doubling, deliberate comb filtering and some stereo-width loss are part of the effect.

## Validation

`Tests/AdvancedTests.cpp` checks neutral paths, measured EQ boost/cut responses, stereo consistency, extreme control automation, selector continuity, and flanger impulse-tail decay at 44.1, 48, 96, and 192 kHz. Synthetic resonance measurements verify attenuation of a 3 kHz peak over noise, preservation of a remote 500 Hz tone, stereo linking, a smooth return to exact dry, and minimal change to both plain noise and isolated steady tones. Modulation tests also check audible wet contributions, stereo movement, and surviving mono sums in all three modes. These numerical tests support stability and intended behavior; they do not replace listening tests on production material, CPU profiling in a DAW, or macOS Audio Unit/Logic Pro validation.
