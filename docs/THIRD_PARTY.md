# Third-party runtime and data

ScpDPI's packet parsing, hostname matching, strategy selection and TCP resegmentation are implemented in `src/service/dpi`. The Windows build uses the x64 WinDivert 2.2.2-A packet driver and helper library from the [WinDivert project](https://github.com/basil00/WinDivert). CMake downloads the pinned release archive and verifies SHA-256 `63cb41763bb4b20f600b6de04e991a9c2be73279e317d4d82f237b150c5f3f15`. The package license is copied beside the runtime as `WinDivert-LICENSE.txt`.

The Russian registry hostname data is downloaded from [bol-van/rulist](https://github.com/bol-van/rulist), pinned to commit `c5470dd0ce0c706c945a938f7694a2634e9d7ea9` and SHA-256 `2c3e871bc8547e923fff557c3b157f492f74f5dedf9566f810e50d0552e36aba`. ScpDPI's supplemental [YouTube list](../src/service/dpi/russia-youtube.txt) and [Discord list](../src/service/dpi/russia-discord.txt) are maintained in this repository.

ScpDPI's own source is licensed under the repository's MIT license. WinDivert and the registry hostname data remain subject to their respective upstream terms; consult the linked repositories and the staged WinDivert license file when redistributing a build.

## GoodbyeDPI behavior review

The [official GoodbyeDPI repository](https://github.com/ValdikSS/GoodbyeDPI) and its [Apache-2.0 license](https://github.com/ValdikSS/GoodbyeDPI/blob/master/LICENSE) were reviewed for high-level behavior and provenance. The review informed an independently written, narrowly scoped IPv4 reset filter for active listed TLS connections. No GoodbyeDPI source, copied function, command preset, packet template, or domain list was added to ScpDPI.

## Independent packet-engine implementation

The local `zapret-discord-youtube-1.10.3` package was reviewed for high-level behavior only: port and host selection, separate TCP and UDP handling, and the presence of distinct strategies for TLS, QUIC and voice-discovery traffic. ScpDPI's parser, host matcher, per-host strategy state, TCP segment construction and UDP discovery handling are implemented in this repository. No zapret source, batch command lines, binary payload templates or domain lists were copied into ScpDPI.

QUIC Initial parsing and key derivation are independently implemented from the published [RFC 9001](https://www.rfc-editor.org/rfc/rfc9001.html) and [RFC 9369](https://www.rfc-editor.org/rfc/rfc9369.html), using the Windows CNG crypto API. The ScpDPI UDP discovery handling follows the public [Discord voice packet layout](https://docs.discord.com/developers/topics/voice-connections) and the STUN message format. A separate decoy gets a distinct Discord SSRC or STUN transaction identifier; the application's original packet is forwarded unchanged. This provenance note describes the engineering work; it is not a legal opinion or a guarantee against claims. The local package did not include a license or notice file in its root. The official upstream repository publishes its license at [zapret/docs/LICENSE.txt](https://github.com/bol-van/zapret/blob/master/docs/LICENSE.txt); any redistribution review should consider the actual package contents and all third-party dependencies independently.
