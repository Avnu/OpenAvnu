# Contributing

Before submitting your patch to review, make sure it doesn't break any unit
test. See the following section for information about how to build and run
the unit tests.

Additionally, if your patch adds new APIs to libavtp, please also provide
patches adding test cases that cover the new APIs.

# Running Unit Tests

Unit tests required libcmocka so make sure you have cmocka packages installed
in your system.

Run the following command to build and run all unit tests. Make sure the build
system files are generated already, see section 'Build' in the README.md file
for more information.

```
$ ninja -C build test
```

# Code Coverage

Meson build system provides some built-in code coverage support based on `lcov`
so make sure you have this package installed on your system in order to
generate coverage reports.

To generate html reports run the following commands:

```
$ rm -rf build/
$ meson build -Db_coverage=true
$ ninja -C build/ test
$ ninja -C build/ coverage-html
```

The coverage report can be found in build/meson-logs/coveragereport/ directory.
