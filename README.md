# What is this template?

This template is an ESP-IDF project with the following changes:
- Arduino library preconfigured.
- ThingsBoard library preconfigured.
- 2 partition OTA preconfigured.
- Clang format file preconfigured.

# Installing ThingsBoard and Arduino library without this template

To install the ThingsBoard and Arduino library without cloning this template just run the two following lines on a shell:

```sh
idf.py add-dependency "espressif/arduino-esp32^3.0.0-alpha2"
idf.py add-dependency "thingsboard/ThingsBoard"
```

The arduino library requires that the following config line is present the sdkconfig file:

```yml
CONFIG_FREERTOS_HZ=1000
```

It can be added to the `sdkconfig.defaults` file so it does not reset when `idf.py menuconfig` is run.