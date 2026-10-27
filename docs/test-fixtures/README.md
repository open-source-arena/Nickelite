# Original cloud-download fixtures

These two small EPUBs contain only original test text and generic metadata. They
were audited for publication; their ZIP CRCs pass. They contain no credentials,
account details or external network links. They are separate from the private
evidence directory and are safe to reuse.

| Provider | Fixture | SHA256 |
|---|---|---|
| Dropbox | [NickelMenuNG-Dropbox-20261026T141758Z.epub](NickelMenuNG-Dropbox-20261026T141758Z.epub) | `ad99300545aca8cb8458c4310eeac782d8112d8882b7957cc4726f6b0368b47e` |
| Google Drive | [NickelMenuNG-GoogleDrive-20261026T141758Z.epub](NickelMenuNG-GoogleDrive-20261026T141758Z.epub) | `e5d898fda4b67543f8b0a7d5d30f42a6d49d2f46f07b8462357065893ce86768` |
| Dropbox fresh-transfer check | [NickelMenuNG-Dropbox-20261027T124826Z.epub](NickelMenuNG-Dropbox-20261027T124826Z.epub) | `cb8c22751f4e2ce699b0390e98d90fc7095bf97f01ef992131aa396f4632720b` |
| 6.0.279225 fresh-transfer check | [000-NickelMenuNG-6.0.279225-20261027.epub](000-NickelMenuNG-6.0.279225-20261027.epub) | `c54600fd9555ec1068826067278d2f3786862059b8f9860d5554dd6515d4f2c1` |
| 6.0.279225 Dropbox fresh-transfer check | [000-NickelMenuNG-Dropbox-6.0.279225-20261027.epub](000-NickelMenuNG-Dropbox-6.0.279225-20261027.epub) | `28fb0ae60daabcb890d1cd20958b70cdf90c5d396e105366368e5b9cf8b1fb97` |

The older 595 build downloaded and opened the first Dropbox fixture with the
`WALNUT` marker, then later panicked on cloud navigation. The next a46 build
panicked on repeat Google Drive entry. Both were retired from physical use.
The current delayed-route `09f8dc6` build reopened Google Drive, switched
between both providers, and downloaded/opened the Google Drive fixture with
its `WALNUT` marker on the physical N365. The new Dropbox fixture ends with
`ACORN` so a fresh download can be distinguished from the cached old file.
The owner uploaded it to the Dropbox `Apps/Rakuten Kobo` folder, downloaded
it on the N365 through NickelMenu → Dropbox, and opened it in My Books with
the expected filename marker and `ACORN` ending. Keep account identities,
file lists, codes and tokens private when recording results.
The 6.0.279225 fixture starts with `000` so it appears near the top of the
name-sorted cloud list. Its page ends with `PECAN` to identify a fresh import.
The physical N365 downloaded and opened it from Google Drive on 6.0.279225.
The distinct Dropbox fixture ends with `CASHEW`; its transfer is pending.
