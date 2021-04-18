TODO
====

- ModifiedTime is the only attribute required for all files.

- Other attributes can be managed using set/remove API calls but requires the (full) file path.

- Attributes not currently managed for directories. ModifiedTime should be set by mkdir call.

- Require API to set/remove attributes on open file or directory - avoids need to keep note of full file path and repeatedly perform path lookup, etc.

- Require API to reconstruct full path for open file (so we don't need to store it).


Add lfs_statcfg, lfs_dir_readcfg functions
    Like lfs_file_opencfg, these allow attributes to be read to reduce disk accesses.
    Testing with Basic_IFS sample reduced readcount from 14109 to 11603.

Increase LFS_CACHE_SIZE from 16 to 32 bytes
    Reduces readcount further to 7667

