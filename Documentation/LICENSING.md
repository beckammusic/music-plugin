# Licensing before distribution

The license of the original GUMMY source is a decision for the project owner. No license has been selected or granted by this document. Add the chosen source license and copyright notices before public release. A project license does not replace the terms of JUCE, its bundled dependencies, Apple SDKs, or the VST3 SDK.

This review inspected the actual **JUCE 8.0.6** checkout used by the build, including `LICENSE.md`, its CMake version declaration, and `modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt`. Preserve the dependency version when making release decisions; newer SDKs can have different terms.

## JUCE 8.0.6

JUCE's root license states that its framework modules are dual licensed under **AGPLv3** and the **commercial JUCE licence**. It links to the [JUCE 8 End User Licence Agreement](https://juce.com/legal/juce-8-licence/) and [GNU AGPLv3](https://www.gnu.org/licenses/agpl-3.0.en.html). The [license file at tag 8.0.6](https://github.com/juce-framework/JUCE/blob/8.0.6/LICENSE.md) is the reviewed source.

Choose and comply with an applicable route before distributing a JUCE-linked GUMMY binary:

- **AGPLv3 route:** license the combined work compatibly with the AGPL, retain required notices, include applicable license text, and provide recipients with Corresponding Source using a method allowed by the license. Corresponding Source includes the material needed to build and install the covered work; a repository link without the required complete source and license compliance is not automatically sufficient. AGPL section 13 also addresses modified versions used for remote network interaction, where applicable. A conventional desktop plugin does not by itself make every unrelated host application AGPL licensed; assess the actual combined work and distribution.
- **Commercial JUCE route:** obtain an entitlement under the JUCE 8 agreement that covers the developer, project, revenue/funding, and distribution conditions. Review the current contract and its incorporated terms directly; do not assume a particular free tier, revenue limit, or perpetual right. A JUCE source download and a successful build do not establish a commercial entitlement. This project does not supply one.

Private development is different from public distribution, but it does not eliminate the need to choose an applicable JUCE licensing basis. Disabling a splash screen or a usage-reporting feature in CMake is a build setting, not permission to ignore licensing.

## VST3 SDK bundled with this pinned JUCE

The SDK's actual `LICENSE.txt` states that referenced SDK files are offered under the **proprietary Steinberg VST3 License** or **GPL version 3**. Some files carry their own embedded notices. See the [file in JUCE 8.0.6](https://github.com/juce-framework/JUCE/blob/8.0.6/modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt).

For the proprietary route, that file says the SDK may not be redistributed in whole or in part without prior written agreement, and explicitly requires obtaining a license agreement signed by Steinberg before publishing software under that route. It also restricts endorsement using Steinberg's or contributors' names and points to separate VST naming/logo guidelines. Obtain and review the applicable agreement rather than assuming a JUCE commercial subscription also grants Steinberg rights.

The alternative is the SDK's GPLv3 route, with its corresponding source, notice, and redistribution requirements. GNU AGPLv3 section 13 and GPLv3 section 13 address combining those licenses; review the resulting obligations for the actual plugin. A closed-source JUCE commercial release does not become compliant with the bundled VST3 SDK solely because JUCE has been licensed commercially.

Do **not** substitute a newer VST3 SDK's MIT license for this pinned SDK's actual terms. Do not add a VST logo or imply Steinberg endorsement without checking the applicable usage rules.

## Other dependencies and Apple requirements

JUCE's root `LICENSE.md` lists its bundled dependencies and links to their individual license texts. Which notices are required depends on the compiled modules and platform. Audit the final configuration and ship the relevant copyright, attribution and license texts with binaries and source distributions. Examples in this pinned checkout include:

| Dependency | License identified by JUCE |
| --- | --- |
| AudioUnitSDK | Apache 2.0 |
| HarfBuzz | Old MIT |
| SheenBidi | Apache |
| zlib and pnglib | zlib |
| jpeglib | Independent JPEG Group |
| VST3 SDK | Proprietary Steinberg / GPLv3, as discussed above |

Additional JUCE modules may compile additional dependencies. The table is not a substitute for the full dependency audit or the actual text, including any required NOTICE files. Retain existing upstream notices when copying or modifying dependency files.

Building with Apple SDKs and distributing macOS software also involves Apple's applicable SDK and developer-program terms. Local ad-hoc signing does not provide a Developer ID identity or notarization. Follow the signing and packaging procedure in [LOGIC_INSTALL.md](LOGIC_INSTALL.md) for distribution, using credentials that remain outside the source tree.

Before a public release, record the selected original-source license, the JUCE licensing basis, the bundled VST3 SDK licensing basis, exact dependency revisions, included third-party notices, and the signed/notarized artifact identities. These licensing decisions and public distribution approval remain with the project owner; no distribution entitlement or legal clearance is asserted here.
