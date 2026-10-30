# KFMon Nickelite assets

These optional assets are built from the `open-source-arena/kfmon` fork at commit
`372432b15933887422aed1975d684126802fd9bc` on branch
`nickelite-fw5-fw6-ota`. The code is based on upstream NiLuJe/KFMon and preserves
upstream daemon, IPC, and watch behavior; the fork adds a guarded OTA stage1
installer/uninstaller for the tested Kobo firmware 5.18.270971 and 6.0.279225
update scripts. The installer does not modify `.adds` or Nickel animator scripts.

Upstream source: <https://github.com/NiLuJe/kfmon>
Fork source: <https://github.com/open-source-arena/kfmon/tree/nickelite-fw5-fw6-ota>

| Asset | SHA-256 |
| --- | --- |
| `KFMon-Nickelite-Install.zip` | `80467510d8df49f6f0385bb6f3453dbc91677ed9b5c16781dcce274106c5f5e4` |
| `KFMon-Nickelite-Uninstall.zip` | `39a95b64d10557a4c048bbf534d4363440ec9b8b6293b99d83a673dc3ff0a053` |
| `KFMon-Nickelite-Defaults.zip` | `9d2dbb1ea57442c4c398d87ef464f49b30456172b51d942c66539ed60ca3d0b1` |

The install and uninstall ZIPs each contain only `.kobo/update.tar`. The defaults
ZIP is separate and optional; it does not overwrite an existing KFMon config.
