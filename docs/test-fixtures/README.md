# Original cloud-download fixtures

These two small EPUBs contain only original test text and generic metadata. They
were audited for publication; their ZIP CRCs pass. They contain no credentials,
account details or external network links. They are separate from the private
evidence directory and are safe to reuse.

| Provider | Fixture | SHA256 |
|---|---|---|
| Dropbox | [NickelMenuNG-Dropbox-20261026T141758Z.epub](NickelMenuNG-Dropbox-20261026T141758Z.epub) | `ad99300545aca8cb8458c4310eeac782d8112d8882b7957cc4726f6b0368b47e` |
| Google Drive | [NickelMenuNG-GoogleDrive-20261026T141758Z.epub](NickelMenuNG-GoogleDrive-20261026T141758Z.epub) | `e5d898fda4b67543f8b0a7d5d30f42a6d49d2f46f07b8462357065893ce86768` |

For each provider separately:

1. Using your own provider session, upload only its matching EPUB to the existing
   Kobo app folder used by that provider's native integration. Keep the existing
   account/folder scope; do not move private files or grant broader access.
2. Open the provider through NickelMenu and use native **Refresh**. Confirm the
   exact fixture filename appears; select and download only that file.
3. Open the downloaded book on the reader. Confirm its provider-specific heading,
   full filename marker, and final word **WALNUT**. Record download and open
   results separately.
4. If checking byte preservation, hash the actual cloud-downloaded EPUB and
   compare with the table. Opening the book or seeing its title does not establish
   equal bytes. Do not overwrite it with a USB copy: that would bypass the cloud
   transfer being tested.

Successful linking and native file listing for both services were owner-reported
on N365/6.0.274403. **These exact fixture download/open steps, byte preservation
and actual file sync have not been verified.** A manual download is not evidence
of automatic or bidirectional sync. Do not use folder-wide sync or Download all
for this bounded test. Keep account identities, file lists, codes and tokens
private when recording the result.
