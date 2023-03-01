# Protobuf?

As I have mentioned on Discord, I want to propose the use of [Protocol Buffers](https://github.com/protocolbuffers/protobuf) (abbreviated as Protobuf) for the future settings storage format for [GP2040-CE](https://github.com/OpenStickCommunity/GP2040-CE). The current approach of directly storing C-structs in flash makes it hard to retain user setting across firmware updates.

Protobuf is an open-source binary serialization format created and mainted by Google, most notably it is used by Googles [gRPC framework](https://grpc.io/). I personally have used Protobuf on multiple projects, both for a network protocol as well as binary storage of game assets.

In this particular case I propose the use of the [Nanopb](https://github.com/nanopb/nanopb) library, which is an implementation of Protobuf for embedded use (small code size, no memory allocations by default, ...).

The main idea is to not store C-struct anymore, but to serialize the settings from C-structs into Protobufs binary format for storage in flash. Conversely, on boot the settings would be deserialized into C-struct again.

Protobuf makes use of schema files that define the structure of the serialized data. This is one simple example of such a schema file:

```protobuf
// Use Protobuf v2
syntax = "proto2";

message BoardOptions {
    optional int32 pinDpadUp = 1;
    optional int32 pinDpadDown = 2;
    optional int32 pinDpadLeft = 3;
    optional int32 pinDpadRight = 4;
    optional int32 pinButtonB1 = 5;
    optional int32 pinButtonB2 = 6;
}
```

The schema is not used a runtime, instead the Nanopb generator is used to generated a `.h` / `.c` pair that will be compiled into the firmware. For each `message` a corresponding `struct` is generated.

The example is almost self-explanatory, but what does this `optional` mean? Singular fields can be marked as `required` or `optional`, we want to mark all fields as `optional` to allow us to remove fields in future versions of our settings (for details see below). In fact `required` was removed from newer versions of the protocol.

Also note that the equal signs do not denote default values but instead assign a unique tag to every field. These tags are used for serialization and should never be used twice, even if the items are removed in future versions of the `message`.

For a more complete description of the Protobuf syntax please refer to the [official documentation](https://protobuf.dev/programming-guides/proto/).

This is the C-struct that is generated from the schema above:
```C
typedef struct _BoardOptions {
    bool has_pinDpadUp;
    int32_t pinDpadUp;
    bool has_pinDpadDown;
    int32_t pinDpadDown;
    bool has_pinDpadLeft;
    int32_t pinDpadLeft;
    bool has_pinDpadRight;
    int32_t pinDpadRight;
    bool has_pinButtonB1;
    int32_t pinButtonB1;
    bool has_pinButtonB2;
    int32_t pinButtonB2;
} BoardOptions;
```

We can use this `struct` the same way we currently use our settings `struct`s, we can read and write to it. We just serialize it to Protobuf again when we are saving. The `has_XXX` flags are generated for every `optional` field, they denote whether the field was actually present in the serialized data or whether the field was initialized with its default value (`0` in this case). We can use this information to support changes to the settings format (again, see below).

# Schema Evolution

The main reason to use Protobuf is the fact that it supports _schema evolution_, it can read serialized messages, even if the underlying schemas do not match exactly. This allows for both forward and backward compatibility.

These are the main rules for schema evolution:
- Unknown fields are ignored during deserialization (i.e. fields that were present when the `message` was serialized but were later removed)
- Missing `optional` fields are initialized with their default values, the corresponding `has_` variable will be set to `false` (i.e. fields that were not present when the `message` was serialized but were added later)

More complex changes can also be handled with additional supporting code, but I will not go into this here. In my experience these cases are rare.

# Example Programs

To proof the feasibility of my proposed approach I have created three test programs that simulate the evolution of a simplified version of the settings struct of GP2040-CE.

All programs share the majority of their code and operate like this:

1. Wait for a BOOTSEL press. This is to allow the user to connect to the USB UART channel to read the upcomping log messages.
2. Log a JSON representation of the current settings.
3. Wait for another BOOTSEL press.
4. Apply some random permutations to the settings.
5. Save the permuted settings to flash.
6. Go to 1

## v0-base

This is the schema used by the `v0-base` program:

```protobuf
syntax = "proto2";

message Settings {
    optional GamepadOptions gamepadOptions = 1;
    optional BoardOptions boardOptions = 2;
}

enum InputMode {
    XInput = 0;
    Switch = 1;
    Hid = 2;
    Config = 255;
}

enum DpadMode {
    Digital = 0;
    LeftAnalog = 1;
    RightAnalog = 2;
}

message GamepadOptions {
    optional InputMode inputMode = 1;
    optional DpadMode dpadMode = 2;
}

message BoardOptions {
    optional int32 pinDpadUp = 1 [default = 2];
    optional int32 pinDpadDown = 2 [default = 3];
    optional int32 pinDpadLeft = 3 [default = 4];
    optional int32 pinDpadRight = 4 [default = 5];
    optional int32 pinButtonB1 = 5 [default = 6];
    optional int32 pinButtonB2 = 6 [default = 7];
}
```

Note the use of the `default` keyword to define default values.

Running this program on a freshly nuked Pi Pico produces the following UART log after two BOOTSEL presses:

```
Loading settings ...
Failed to load settings. Initialize to defaults.


Initial State:
==============

{
    "gamepadOptions":
    {
        "inputMode": 0,
        "dpadMode": 0
    },
    "boardOptions":
    {
        "pinDpadUp": 2,
        "pinDpadDown": 3,
        "pinDpadLeft": 4,
        "pinDpadRight": 5,
        "pinButtonB1": 6,
        "pinButtonB2": 7
    }
}

Performing random modifications ...

Modified State:
===============

{
    "gamepadOptions":
    {
        "inputMode": 0,
        "dpadMode": 0
    },
    "boardOptions":
    {
        "pinDpadUp": 54,
        "pinDpadDown": 3,
        "pinDpadLeft": 12,
        "pinDpadRight": 49,
        "pinButtonB1": 8,
        "pinButtonB2": 32
    }
}

Saving settings ...

Waiting for BOOTSEL press ...

Loading settings ...


Initial State:
==============

{
    "gamepadOptions":
    {
        "inputMode": 0,
        "dpadMode": 0
    },
    "boardOptions":
    {
        "pinDpadUp": 54,
        "pinDpadDown": 3,
        "pinDpadLeft": 12,
        "pinDpadRight": 49,
        "pinButtonB1": 8,
        "pinButtonB2": 32
    }
}

Performing random modifications ...

Modified State:
===============

{
    "gamepadOptions":
    {
        "inputMode": 0,
        "dpadMode": 1
    },
    "boardOptions":
    {
        "pinDpadUp": 3,
        "pinDpadDown": 10,
        "pinDpadLeft": 12,
        "pinDpadRight": 52,
        "pinButtonB1": 11,
        "pinButtonB2": 30
    }
}

Saving settings ...

Waiting for BOOTSEL press ...
```

Because the Pi Pico was nuked, no valid Protobuf data could be read, instead a settings `struct` initialized with all default values is returned from the deserialization function. This can be detected by code and more elaborate default initialization could be performed, as suggested by this code:

```C++
if (loadSettings(settings) == LoadSettingsResult::DefaultInit)
{
    printf("Failed to load settings. Initialize to defaults.\n");

    // Perform additional manual default initialization here ...
}
```

The subsequent load correctly reads the randomly modified values that were written before.

## v1-added_fields

This program changes the schema: support for the `BuzzerAddon` and `invertXAxis` and `invertYAxis` were added:

```protobuf
syntax = "proto2";

import "nanopb.proto";

message Settings {
    optional GamepadOptions gamepadOptions = 1;
    optional BoardOptions boardOptions = 2;
    optional BuzzerAddon buzzerAddon = 3;
}

enum InputMode {
    XInput = 0;
    Switch = 1;
    Hid = 2;
    Config = 255;
}

enum DpadMode {
    Digital = 0;
    LeftAnalog = 1;
    RightAnalog = 2;
}

message GamepadOptions {
    optional InputMode inputMode = 1;
    optional DpadMode dpadMode = 2;
    optional bool invertXAxis = 3;
	optional bool invertYAxis = 4;
}

message BoardOptions {
    optional int32 pinDpadUp = 1 [default = 2];
    optional int32 pinDpadDown = 2 [default = 3];
    optional int32 pinDpadLeft = 3 [default = 4];
    optional int32 pinDpadRight = 4 [default = 5];
    optional int32 pinButtonB1 = 5 [default = 6];
    optional int32 pinButtonB2 = 6 [default = 7];
}

message BuzzerAddon {
    optional bool enabled = 1;
    optional int32 pin = 2 [default = -1];
    optional uint32 volume = 3 [default = 100];
}
```

Running this program on the same Pi Pico used earlier produces this log:

```
Loading settings ...


Initial State:
==============

{
    "gamepadOptions":
    {
        "inputMode": 0,
        "dpadMode": 1,
        "invertXAxis": false,
        "invertYAxis": false
    },
    "boardOptions":
    {
        "pinDpadUp": 3,
        "pinDpadDown": 10,
        "pinDpadLeft": 12,
        "pinDpadRight": 52,
        "pinButtonB1": 11,
        "pinButtonB2": 30
    },
    "buzzerAddon":
    {
        "enabled": false,
        "pin": -1,
        "volume": 100
    }
}

Performing random modifications ...

Modified State:
===============

{
    "gamepadOptions":
    {
        "inputMode": 0,
        "dpadMode": 1,
        "invertXAxis": false,
        "invertYAxis": false
    },
    "boardOptions":
    {
        "pinDpadUp": 6,
        "pinDpadDown": 59,
        "pinDpadLeft": 34,
        "pinDpadRight": 10,
        "pinButtonB1": 11,
        "pinButtonB2": 16
    },
    "buzzerAddon":
    {
        "enabled": true,
        "pin": 28,
        "volume": 100
    }
}

Saving settings ...

Waiting for BOOTSEL press ...
```

Note that all previously stored values were correctly deserialized, all new fields were initialized with the default values given in the schema. This is exactly what we want.

Again we could implement more elaborate default initialization as suggested by this code snippet:

```C++
if (!settings.has_buzzerAddon)
{
    // Perform additional manual default initialization for buzzerAddon here ...
}
```

As previously stated the `has_XXX` flags indicate whether a field was deserialized of default-initialized.

## v2-removed_fields

Finally the `v2-removed_fields` program removes fields from the schema. In this version `boardOptions` and `invertXAxis` do not exist anymore:

```protobuf
syntax = "proto2";

message Settings {
    optional GamepadOptions gamepadOptions = 1;
    // optional BoardOptions boardOptions = 2;
    optional BuzzerAddon buzzerAddon = 3;
}

enum InputMode {
    XInput = 0;
    Switch = 1;
    Hid = 2;
    Config = 255;
}

enum DpadMode {
    Digital = 0;
    LeftAnalog = 1;
    RightAnalog = 2;
}

message GamepadOptions {
    optional InputMode inputMode = 1;
    optional DpadMode dpadMode = 2;
    // optional bool invertXAxis = 3;
	optional bool invertYAxis = 4;
}

message BuzzerAddon {
    optional bool enabled = 1;
    optional int32 pin = 2 [default = -1];
    optional uint32 volume = 3 [default = 100];
}
```

Executing this program on yet again the same Pi Pico produces this log:

```
Loading settings ...


Initial State:
==============

{
    "gamepadOptions":
    {
        "inputMode": 0,
        "dpadMode": 1,
        "invertYAxis": false
    },
    "buzzerAddon":
    {
        "enabled": true,
        "pin": 28,
        "volume": 100
    }
}

Performing random modifications ...

Modified State:
===============

{
    "gamepadOptions":
    {
        "inputMode": 0,
        "dpadMode": 1,
        "invertYAxis": false
    },
    "buzzerAddon":
    {
        "enabled": true,
        "pin": 28,
        "volume": 17
    }
}

Saving settings ...

Waiting for BOOTSEL press ...
```

The values that still exis are correctly restored. The fields that were removed are just ignored during deserialization.

# Pros / Cons

Finally, I want to give a short overview over what I perceive as the advantages and disadvantages of this approach:

| Pro / Con | Description |
|:---------:| ----------- |
| :white_check_mark: | This solves the problem of migrating settings across firmware updates with very little effort and little room for error. It is not even strictly necessary to use a version number, the option is there but in many cases it will just work. |
| :white_check_mark: | Serialization uses a compact format using very little flash space. There is no longer the need to carve out sections of flash for the various sub-objects. I have written a [proto file](https://github.com/mthiesen/nanopb-test/blob/master/proto/complete.proto) that approximates the current settings in GP2040-CE. The maximum possible serialized size is 1,979 bytes, this is far less than the 8 kb that are currently reserved for settings. |
| :white_check_mark: | Protobuf is very mature technology and is relied on by many companies and projects. Also Nanopb is explicitly meant to be used in a embedded environment and therefore uses very little resources. |
| :white_check_mark: | I am confident that with a little bit of extra work and possibly changes to the proto-generator, I can make conversion from and to JSON working. This would be ideal for exporting / importing of settings on the webconfig. The example programs can already convert to JSON using some macro-magic [here](https://github.com/mthiesen/nanopb-test/blob/cea3c88dcc01e81401709ecf692e88a42fa6c991/common/settings.cpp#L162). |
| :x: | Introducing another technology further complicates the build process. The Nanobp code generator needs both [Python](https://www.python.org/) and `protoc`, the Protobuf compiler. Also, contributers will need to deal with this additional element. |
| :x: | This is not a silver-bullet. While Protobuf would simplify the issue a lot, there is still the need for careful planning and proper testing on each new release. Relying of Protobuf too much could lead to a false sense of security. |
