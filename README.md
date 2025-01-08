# SimpleIOUSB

A single-file C library to abstract away the annoyances of IOKit and IOUSB.

## Author's Note

For a number of years, I've copied and pasted snippets of code (that have now
evolved into this library) between projects of mine. After doing this enough
times, I wanted to finally put something proper together.

That said, this library is far from feature-complete and only really contains
abstractions for the things I've needed to do. However, hopefully it can still
be useful for you.

## Design Goals

Foremost, this library is intended to be simple and easy to use. However, a few
other things that were in mind when designing this library were:

- keeping the code freestanding, so that this library may be easily dropped
  into a project and forgotten;
- providing a pure C API, for maximum portability and so that bindings to other
  languages may be easily written at a later time; and
- returning ("bubbling-up") actual IOKit return codes wherever possible, as to
  not obscure what went wrong if a function fails.

## Usage

The example applications exist to illustrate how the library is used (and also
to catch breaking changes). Most of the API should be pretty obvious, but I've
tried to document the less-obvious parts in the header.

## Building

This library has been designed so that you can simply drop the two source files
into your own project; you'll just need to link against Foundation and IOKit.

However, you may also build it using CMake if you want to add it as a submodule
or install it to your system.

## License

Copyright © 2022-2025 Jon Palmisciano. All rights reserved.

Licensed the BSD 3-Clause license; the full terms of the license can be found
in [LICENSE.txt](LICENSE.txt).
