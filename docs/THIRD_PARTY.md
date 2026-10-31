# Third-party runtime

Windows console builds stage the x64 `GoodbyeDPI 0.2.3rc3-2` release from the upstream [GoodbyeDPI project](https://github.com/ValdikSS/GoodbyeDPI). The archive is pinned to SHA-256 `37f96b32d050dadcc930a639eba68e1ccd57ed5c04a5f77dfca908f01905a4c5` and contains GoodbyeDPI, WinDivert, and the upstream Russian and YouTube block lists.

The build copies the upstream `licenses` directory into `scpdpi-dpi/licenses` beside the executable. GoodbyeDPI is Apache-2.0; WinDivert is LGPL-3.0 (or GPL-2.0). ScpDPI's MIT license applies only to ScpDPI-authored files. The corresponding source and license terms are available from the linked upstream repositories.

ScpDPI adds its own Discord host list at `src/service/dpi/scpdpi-discord.txt`.
