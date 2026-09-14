# Release channels

The repository has two public release channels:

- `main`: complete DLAssAss 5 Tool application releases, tagged `v.1.x.y`.
- `standalone-addon`: manual-install addon releases, tagged with the addon's
  Windows build version and containing only `renodx-dlss5-super-anus.addon64`.

Every future public release must publish the complete application package from
`main` and the matching loose addon from `standalone-addon`. NVIDIA runtime DLLs
must never be committed or attached to either channel.

## Stable standalone builds

| Addon version | SHA-256 |
| --- | --- |
| 1.0.0 | `54EFEFB405491ECAA6580A10DEAD86ED94C230DD9C29F6BFFF09C0429A625F77` |
| 1.0.1 | `0845873E460BB87C16C6EAC9DB462DDC161BF2B895BB9BFE63B5152AE40F96E3` |
| 1.0.2 | `63447B8A5607CF32D549AE01FB37899B1C387DBC18AAC4480E7FA2251AC6FAB4` |
| 1.0.3 | `74340C05CFEF6C2C3D77BF78BFDFB3BF9939CFFCCE9222B4E40073023B29C03C` |
| 1.0.3.12 | `21C61735076FCB1FC0F439542F34D003F172D91D43253D5C1884EBD90C43714A` |
| 1.0.5.16 | `750982FCF31AC8F912561AC72BDEBDE019DCCD3F427BAF3FE24ABF2F96B28139` |
| 1.0.6.17 | `8F282AC07D6F430580626A4DA375E2501912C450CF6D22B857406E91874A9622` |

The 1.0.3 preview builds 13–15 and unversioned diagnostic candidates remain
development artifacts and are not standalone stable releases.
