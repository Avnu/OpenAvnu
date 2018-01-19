# About

Open source implementation of Audio Video Transport Protocol (AVTP) specified
in IEEE 1722-2016 spec.

Libavtp is under BSD License. For more information see LICENSE file.

# Build

Before building libavtp make sure you have all the required software installed
in your system. Below are the requirements and their tested versions:

* Meson >= 0.43
* Ninja >= 1.8.2

The first step to build libavtp is to generate the build system files.

```
$ meson build
```

Then build libavtp by running the following command. The building artifacts
will be created under the build/ in the top-level directory.

```
$ ninja -C build
```

To install libavtp on your system run:
```
$ sudo ninja -C build install
```

# AVTP Formats Support

AVTP protocol defines several AVTPDU type formats (see Table 6 from IEEE
1722-2016 spec). Libavtp doesn't support all of them yet. The list of supported
formarts is:
* AAF (PCM encapsulation only)
