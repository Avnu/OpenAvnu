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
