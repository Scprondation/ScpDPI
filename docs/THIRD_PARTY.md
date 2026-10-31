# Third-party runtime and data

ScpDPI's packet parsing, hostname matching, strategy selection and TCP resegmentation are implemented in `src/service/dpi`. The Windows build uses the x64 WinDivert 2.2.2-A packet driver and helper library from the [WinDivert project](https://github.com/basil00/WinDivert). CMake downloads the pinned release archive and verifies SHA-256 `63cb41763bb4b20f600b6de04e991a9c2be73279e317d4d82f237b150c5f3f15`. The package license is copied beside the runtime as `WinDivert-LICENSE.txt`.

The Russian registry hostname data is downloaded from [bol-van/rulist](https://github.com/bol-van/rulist), pinned to commit `c5470dd0ce0c706c945a938f7694a2634e9d7ea9` and SHA-256 `2c3e871bc8547e923fff557c3b157f492f74f5dedf9566f810e50d0552e36aba`. ScpDPI's supplemental [YouTube list](../src/service/dpi/russia-youtube.txt) and [Discord list](../src/service/dpi/russia-discord.txt) are maintained in this repository.

ScpDPI's own source is licensed under the repository's MIT license. WinDivert and the registry hostname data remain subject to their respective upstream terms; consult the linked repositories and the staged WinDivert license file when redistributing a build.
