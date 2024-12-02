TinyDir
=======
[![CMake](https://github.com/cxong/tinydir/actions/workflows/cmake.yml/badge.svg)](https://github.com/cxong/tinydir/actions/workflows/cmake.yml)
[![Release](http://img.shields.io/github/release/cxong/tinydir.svg)](https://github.com/cxong/tinydir/releases/latest)

Lightweight, portable and easy to integrate C directory and file reader. TinyDir wraps dirent for POSIX and FindFirstFile for Windows.

Windows unicode is supported by defining `UNICODE` and `_UNICODE` before including `tinydir.h`.

Example
=======

There are two methods. Error checking omitted:

```C
tinydir_dir dir;
tinydir_open(&dir, "/path/to/dir");

while (dir.has_next)
{
	tinydir_file file;
	tinydir_readfile(&dir, &file);

	printf("%s", file.name);
	if (file.is_dir)
	{
		printf("/");
	}
	printf("\n");

	tinydir_next(&dir);
}

tinydir_close(&dir);
```

```C
tinydir_dir dir;
int i;
tinydir_open_sorted(&dir, "/path/to/dir");

for (i = 0; i < dir.n_files; i++)
{
	tinydir_file file;
	tinydir_readfile_n(&dir, &file, i);

	printf("%s", file.name);
	if (file.is_dir)
	{
		printf("/");
	}
	printf("\n");
}

tinydir_close(&dir);
```

See the `/samples` folder for more examples, including an interactive command-line directory navigator.

Language
========

ANSI C, or C90.

Platforms
=========

POSIX and Windows supported. Open to the possibility of supporting other platforms.

License
=======

Simplified BSD; if you use tinydir you can comply by including `tinydir.h` or `COPYING` somewhere in your package.

Known Limitations
=================

- Limited path and filename sizes
- [Possible race condition bug if folder being read has changing content](https://github.com/cxong/tinydir/issues/13)
- Does not support extended-length path lengths in Windows - paths are limited to 260 characters. See <https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=registry>
