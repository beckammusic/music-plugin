# Dynamics algorithms

`TransientShaper` and `Compressor` are standard C++17 modules. `prepare(sampleRate)`
sets time constants, `reset()` clears state, and `process(left, right, settings)`
processes one stereo frame in place. They add no latency and allocate no memory in
`process`. The containing engine handles module bypass. In mono, pass the same
sample into both channels and use either result.

## TRANSIENT

The larger absolute stereo sample feeds linked fast (0.4 ms attack / 8 ms release)
and slow (20 ms attack / 120 ms release) envelopes. Their positive difference,
normalized to the fast envelope, estimates the onset. The complementary activity
estimates the body and decay. Separate attack and sustain control envelopes smooth
these estimates at 0.25/3 ms and 3 ms respectively. Attack spans up to ±12 dB on
onsets; Sustain spans up to ±9 dB on the body. These bounds describe each control
alone; overlapping detector activity can combine them. The common gain preserves
the stereo image. Detection action fades below approximately -80 dBFS to reduce
noise pumping. Initial neutral settings are sample-exact unity while the detector
continues running. Both macro controls interpolate over 20 ms during automation.

This is a broad musical envelope shaper, without lookahead, source separation, or
automatic loudness normalization. Large sustain boosts can increase noise between
notes; lower the macro when needed. Its level-relative onset estimate makes attack
behavior substantially less dependent on absolute recording level than a fixed
threshold design.

## COMPRESS

All three characters use feed-forward detection and gain reduction in decibels.
Above the knee, reduction is `(level - threshold) * (1 - 1 / ratio)`. Inside a
width-K knee, reduction is `(1 - 1 / ratio) * (level - threshold + K/2)^2 / (2*K)`;
below the knee it is zero. This produces continuous value and slope at both knee
boundaries, with exact unity at 1:1 before makeup gain. There is no proprietary
emulation or copied compressor implementation.

| Type | Detector | Knee | Gain envelope |
| --- | --- | --- | --- |
| Smooth | 10 ms one-pole energy average, square-rooted to RMS | 6 dB | User attack, gently program-dependent release |
| Punch | Instantaneous linked peak | 1 dB | Attack lengthens up to 2.5× on high-crest onsets, preserving the leading edge; release is 0.7× user time |
| Glue | 65% of a 5 ms RMS detector + 35% of a 0.5/30 ms peak envelope | 4 dB | User attack and 1.15× release, with slower recovery after sustained reduction |

RMS energy uses the louder channel's instantaneous squared sample. This conservative
linking means a loud signal on either side controls the same gain on both channels;
it avoids image shifts and leaves the quieter side's stereo ratio intact. RMS levels
are true RMS over the specified exponentially weighted window, without a sine-wave
calibration offset. A steady sine consequently reads about 3 dB below its peak in
Smooth mode. Timing values are one-pole time constants, not time to complete gain
reduction. Detector integration also contributes to onset response in RMS modes.

Automatic release uses a separate reduction-history envelope with 50 ms attack and
1 second recovery. It scales release between 0.55× for brief gain reduction and 2×
for sustained reduction, in addition to the type's release multiplier. This is
bounded, has no signal-path feedback, and is smoothly introduced when Auto changes.
All continuous controls interpolate over 20 ms. Every mode runs continuously, and
10 ms mode-weight transitions blend their reduction in dB so switching does not
reset the envelopes or abruptly change the gain.

The gain-reduction meter reports positive linked attenuation before makeup and
parallel mix. Makeup is fixed user gain; it is not an automatic loudness matcher.
Parallel mix combines the input and processed sample directly because this module
has zero latency. No lookahead or brick-wall limiting is claimed: the attack
deliberately permits transients to pass before full gain reduction develops.

`Tests/DynamicsTests.cpp` verifies sample-exact neutral transient processing,
impulse attack, a drum-like decay, analytic threshold/ratio/knee behavior, makeup,
stereo linking, parallel mix, attack and manual release timing, program-dependent
release, differences between detectors, silence/reset behavior, extreme inputs,
and supported sample rates. It can be linked into any C++17 runner that calls
`void testDynamics()`.
