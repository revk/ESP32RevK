# Default LED codes

The status LED can be overridden, but has some defaults from the `revk_blinker()` function.

## Overrides

These apply even if the application has set another LED sequence or `dark` is set.

**These are solid, non blinking/pulsing**

|Colours|Meaning|
|-------|-------|
|Green|First 2 seconds from power up|
|Yellow|First press of factory reset (if you wait 3 seconds with no more presses then it will reboot)|
|Orange|Second press of factory reset|
|Red|Third press of factory reset (factory reset and reboot is now done)|

## Normal

These are off when in `dark` mode. These pulse on and off, cycling the colours, fast if no WiFi, slow if WiFi.

The following are picked, first in list that applies

|Colours|Meaning|
|-------|-------|
|White|Rebooting or software upgrade|
|Red/White|No WiFi configured|
|Application sequence|Whatever set by the application, if set|
|Blue/White|WiFi is in AP+STA mode|
|Cyan/White|WiFi is in AP only mode|
|Magenta/White|WiFi is off|
|Yellow/White|No IP|
|Black|If `dark` is set|
|Rainbow|Default cycle through 6 colours|
