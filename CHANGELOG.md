# Changelog

Changes happening to Gumball will be available here.
**Format: MM-DD-YY**  

## Version 1.1.0 - 08-03-26
### Added
* `mv` command to rename files, doesn't move files yet.
### Files Changed
* `/kernel/fs/fs.c`: `void fs_rename_file(...) {`
* `/kernel/fs/fs.h`: `void fs_rename_file(...);`
* `/kernel/shell/shell.c`: `term_write("...")`...`else if (str_equal(cmd, "mv")) {`

## Initial Commit version 1.0.0 - 08-03-26
No changes. Minimal features (view initial commit of README)