# Contributing

If you have a bug fixed or added support for some AVTP feature, your patches
are welcomed! In order to get your patches merged faster, please follow the
guidelines:

* Before submitting your patch to review, make sure it doesn't break any unit
test. See section 'Running Unit Tests' for information about how to build and
run unit tests.

* Besides the bugfix/feature itself, also provide unit test covering the code
you're contributing. See section 'Code Coverage' to check how you can easily
generate coverage reports and see where you need to work on to get your code
covered.

* If your patch adds new public APIs to libavtp, please also provide patches
adding example applications (or modify an existing one if it makes sense)
which demonstrate how to use the new APIs.

* Make sure the author's name and email are set properly.

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
