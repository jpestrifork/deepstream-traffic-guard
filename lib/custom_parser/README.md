# Custom LPR Parser

This directory contains a custom inference parser for licence-plate recognition.

## Build

```bash
make
```

This produces `libnvdsinfer_custom_impl_lpr.so` in the current directory.

## DeepStream config wiring

In the DeepStream pipeline YAML (e.g. `configs/deepstream_config.yml`) the secondary GIE block must point `custom-lib-path` at the built shared library. Use a path relative to the working directory from which the application is launched, for example:

```yaml
custom-lib-path: lib/custom_parser/libnvdsinfer_custom_impl_lpr.so
```

or an absolute path if the binary is installed elsewhere.
