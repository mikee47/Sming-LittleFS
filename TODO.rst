TODO
====

- ModifiedTime is the only attribute required for all files.

- Other attributes can be managed using set/remove API calls but requires the (full) file path.

- Attributes not currently managed for directories. ModifiedTime should be set by mkdir call.

- Require API to set/remove attributes on open file or directory - avoids need to keep note of full file path and repeatedly perform path lookup, etc.

- Require API to reconstruct full path for open file (so we don't need to store it).

