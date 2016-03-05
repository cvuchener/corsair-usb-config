Corsair USB configuration tool
==============================

This is configuration tool for Corsair gaming keyboards using the older USB protocol (not the newer using pure HID configuration, e.g. RGB keyboards, supported by [ckb](https://github.com/ccMSC/ckb)). Currently supported devices are:
 - Vengeance K90
 - Raptor K40

Compilation
-----------

You need libusb and jsoncpp and a C++ compiler. Simply use `make` to build the executable.


Usage
-----

`./corsair-usb-config [options] command`

Commands are:
 - `list`: List supported devices.
 - `mode get|set [new_value]`: Get or set the macro playback mode. In `HW` mode, macro stored in the hardware will be used. In `SW` mode, key will only send their respective key codes.
 - `backlight get|set [new_value]`: Get or set the brightness of the backlight (from 0 to 3).
 - `current-profile get|set [new_value]`: Get or set the current profile (from 1 to 3).
 - `profile-color get|set index [new_value]`: Get or set the profile `index` color. Colors are encoded in a 24 bits hexadecimal number (R8G8B8).
 - `send-macros index [file]`: Send macros to the hardware profile `index` (from 1 to 3). If `file` is missing, macros are read from the standard input.

Options are:
 - `-d address`: Use this device instead of first found.
 - `-l layout`: Use layout for converting string to key codes (see KeyUsage.cpp). Available layouts are: `AZERTY-Fr`.
 - `-h`: Print help.


Profile file format
-------------------

Profile file use JSON syntax. The profile is an array of objects where each object describe the settings for a key.

The member for a key settings are:
 - **key**: the key to configure (usually `G*`). This member is mandatory.
 - **repeat_mode**: how the macro is repeated. Accepted values are:
   - *fixed* (default): the macro is played a fixed number of time when the key is pressed.
   - *hold*: the macro is repeated while the key is held.
   - *toggle*: the macro is repeated until the key is pressed again.
 - **type**: the type of settings for the key. Accepted values are:
   - *none*: the key produces its normal key code.
   - *key*: the key produces another key's code.
   - *macro* (default): the key plays a macro.
 - **new_key**: the key whose key code will be used with *key* type.
 - **repeat_count**: the number of time the macro is played in *fixed* mode. Only used by *macro* type. Default is 1.
 - **macro**: an array of macro items. Mandatory for *macro* type. Members for macro items are:
   - **key**: for a key event, the value is the key whose event is played. The item must have a **pressed** member if this one is set.
   - **pressed**: a boolean: *true* for press event, *false* for a release event.
   - **delay**: create a delay in the macro. The delay is given in milliseconds.

See the list of accepted keys in [KeyUsage.cpp](KeyUsage.cpp).

See [default.json](default.json) or [example.json](example.json) for example profiles.

Because of the hardware, the **key** member is not always used as it should.  It is only used in the first sixteen items. The 17th and 18th always match G17 and G18 and following do nothing. Also if the *Gn* key is not configured but there is a *n* th settings item it will be used for both configured key and the *Gn* key.

License
-------

This program is licensed under GNU GPL.
