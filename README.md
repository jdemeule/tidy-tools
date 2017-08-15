# tidy-tools

## Description

_tidy-tools_ is a small project to transform C++ code.

This project is split on 2 parts:
* the effective C++ tools based on Clang under _tools_ folder
* the driver in C# driver to apply tools on files listed in a `compile_commands.json` file.


## How to compile
Under tidy-tools folder:
```
$ mkdir build-dir
$ cd build-dir
$ ../configure.sh # adapt it according with your platform
$ make -j 4
```

`run-tidy` could be build with Visual Studio (10 and above) or Mono tools.

## Tools provided
* clang-tidy: Apply a clang tidy transformation.
* early-return: Apply the 'early return' pattern on the function of the file
where possible.

## Usage

`run-tidy` will run thought files described in a `compile_commands.json` and
apply transformation on them.

It is possible to specify a file, a list of file or some filter where to apply
the transformations.
For example:
```
$ run-tidy --foo xxx.cpp
```
Will apply `foo` tranformation to `xxx.cpp`.


```
$ run-tidy --foo bar/zzz
```
Will apply `foo` tranformation to every file under `bar/zzz`.



Full command list is:
```
$ run-tidy --help
Usage: run-tidy [options] [files [files...]]

Runs transformation (clang-tidy, small-tidy, internal ones) over all files in a
compilation database.
Requires clang-tidy, clang-apply-replacements and small-tidy in $PATH.

Version: 1d

Positional arguments:
  files               files to be process (anything that will match within
                      compilation database contents)

Optionnal arguments:
  -h, --help                 show this help message
  -j, --jobs=VALUE           number of concurent jobs
      --outputdir=VALUE      patches output directory
      --apply, --fix         apply patches
      --apply-until-done     apply until transformations do nothing
      --p4-edit              call p4 edit before applying patches
      --debug-me             call debugger on run-tidy
      --verbose              be more verbose.
      --quiet                be quiet.
      --clang-tidy=VALUE     Run clang-tidy transformation(s).
                               Call 'clang-tidy -list-checks -checks="*"' for
                               the full list.
      --early-return         Tranform function' ifs to early return where
                               possible.
                               Only small if statement (less than 4 inner
                               statements) and without any return could be
                               transform.
      --sample               Sample transform
```


## Note

This project is licensed under the terms of the MIT license.
