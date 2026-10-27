# Original cloud-download fixtures

These two small EPUBs contain only original test text and generic metadata. They
were audited for publication; their ZIP CRCs pass. They contain no credentials,
account details or external network links. They are separate from the private
evidence directory and are safe to reuse.

| Provider | Fixture | SHA256 |
|---|---|---|
| Dropbox | [NickelMenuNG-Dropbox-20261026T141758Z.epub](NickelMenuNG-Dropbox-20261026T141758Z.epub) | `ad99300545aca8cb8458c4310eeac782d8112d8882b7957cc4726f6b0368b47e` |
| Google Drive | [NickelMenuNG-GoogleDrive-20261026T141758Z.epub](NickelMenuNG-GoogleDrive-20261026T141758Z.epub) | `e5d898fda4b67543f8b0a7d5d30f42a6d49d2f46f07b8462357065893ce86768` |

**Do not repeat these cloud actions on either the old 595 build or a46.** On
N365/6.0.274403, the owner initially reported both native lists opened and
stayed responsive for about 30 seconds. Dropbox then passed this fixture test:
the owner uploaded
`NickelMenuNG-Dropbox-20261026T141758Z.epub` to the existing `Apps/Rakuten Kobo`
folder, selected and downloaded it from the native Dropbox list, then opened it
from My Books and verified the matching marker and final word **WALNUT**.
That Dropbox interaction completed with no reported panic. Later Google Drive
selections caused panic/restarts, followed by Dropbox selections causing
panic/restarts. The initial five crash logs came from the old installed build;
new candidate-era records 05 and 06 log the native Google Drive call returning
before Qt shared-pointer/event-dispatch SIGSEGVs. The newer source candidate has
since been installed with the correct package path and consumed USB files. Its
first Google Drive selection from Home opened
a list; the second selection and another after restart caused panic/restarts.
Root stopped cloud testing and restored containment; the reader is responsive.
The candidate Dropbox route and Google Drive fixture download/open were not
tested. Byte-for-byte preservation and actual file sync were not verified. The c980d6c containment rollback was installed in between; Dropbox's disabled
message returned to a responsive Home screen. Do not use either cloud action until a reviewed fix is installed and verified.
Do not use folder-wide sync or Download all. Keep account identities, file lists,
codes and tokens private when recording results.
