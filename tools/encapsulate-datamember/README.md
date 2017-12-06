# encapsulate-datamember

## Description

_encapsulate-datamember_ is a tools to generate and replace usage of direct
datamember access.
It is perfectly fine to use directly datamember in C++, however, for some
refactoring purpose you can need to encapsulate them in some functions
(getter/setter).
In the same time, writting this kind of transformation could also be a fun
challenge.


## Usage

General help
```
$ encapsulate-datamember -help
```

To encapsulate the member `x` of the class `foo` in the namespace `abc`:
```
$ encapsulate-datamember -names="abc::foo::x" xyz.cpp
```

To generate getter/setter in *camel* style (default):
```
$ encapsulate-datamember -camelCase -names="abc::foo::x" xyz.cpp
```

To generate getter/setter in *snake_case* style:
```
$ encapsulate-datamember -snake_case -names="abc::foo::x" xyz.cpp
```


## Note

This project is licensed under the terms of the MIT license.
