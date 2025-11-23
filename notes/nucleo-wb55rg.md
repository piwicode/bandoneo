# Getting started with nuccleo-wb55rt development board

Checkout all examples with git:

```
git clone --recurse-submodules https://github.com/STMicroelectronics/STM32CubeWB.git
```

Display serial with:

```
minicom -D /dev/ttyACM1
```

You will have to convert carriage return either in the session pressing `CTRL-A, Z`
then `U`, or permanently with `sudo minicom -s` then `Screen and keyboard`
then `T - Add carriage return`, and `Save setup as dfl`.

In the examples the preprocessor variables `CFG_DEBUG_APP_TRACE`, `CFG_DEBUG_BLE_TRACE`
should be set to `1` in `app_conf.h` for traces to be logged, `CFG_LPM_SUPPORTED` to `1`
for the debugger to work in all modes.
