# Stable parameter contract

Version 1.0.0; parameter version hint 1. Values use native units. All 34 controls are exposed to the host. Timings and ratio use skewed normalized ranges. Module Enabled=1 runs processing; master Bypass=1 bypasses the chain.

| ID | Name | Min | Max | Default | Unit / choices |
|---|---|---:|---:|---:|---|
| `input` | Input | -24 | 24 | 0 | dB |
| `output` | Output | -24 | 24 | 0 | dB |
| `mix` | Global Mix | 0 | 100 | 100 | % |
| `bypass` | Master Bypass | 0 | 1 | 0 | bool |
| `quality` | Oversampling | 0 | 2 | 1 | Off|2x|4x |
| `transientOn` | Transient Enabled | 0 | 1 | 1 | bool |
| `attack` | Transient Attack | -100 | 100 | 0 |  |
| `sustain` | Transient Sustain | -100 | 100 | 0 |  |
| `compOn` | Compressor Enabled | 0 | 1 | 1 | bool |
| `threshold` | Threshold | -60 | 0 | 0 | dB |
| `ratio` | Ratio | 1 | 20 | 1 | :1 |
| `compAttack` | Compressor Attack | .1f | 100 | 10 | ms |
| `release` | Release | 10 | 2000 | 150 | ms |
| `makeup` | Makeup | -12 | 24 | 0 | dB |
| `autoRelease` | Auto Release | 0 | 1 | 1 | bool |
| `compMix` | Compressor Mix | 0 | 100 | 100 | % |
| `compType` | Compressor Type | 0 | 2 | 0 | Smooth|Punch|Glue |
| `colorOn` | Color Enabled | 0 | 1 | 1 | bool |
| `colorDrive` | Color Drive | 0 | 100 | 0 |  |
| `colorMix` | Color Mix | 0 | 100 | 100 | % |
| `colorType` | Color Type | 0 | 2 | 0 | Tape|Tube|Color |
| `toneOn` | Tone Enabled | 0 | 1 | 1 | bool |
| `body` | Body | -100 | 100 | 0 |  |
| `presence` | Presence | -100 | 100 | 0 |  |
| `air` | Air | -100 | 100 | 0 |  |
| `smoothOn` | Smooth Enabled | 0 | 1 | 1 | bool |
| `smooth` | Resonance Suppression | 0 | 100 | 0 |  |
| `motionOn` | Motion Enabled | 0 | 1 | 1 | bool |
| `motionAmount` | Motion Amount | 0 | 100 | 0 |  |
| `motionType` | Motion Type | 0 | 2 | 0 | Chorus|Flanger|Doubler |
| `clipOn` | Clip Enabled | 0 | 1 | 0 | bool |
| `clipDrive` | Clip Drive | 0 | 24 | 0 | dB |
| `ceiling` | Clip Ceiling | -12 | 0 | 0 | dBFS |
| `clipMode` | Clip Mode | 0 | 1 | 0 | Hard|Soft |
