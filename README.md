# mauzr-esp32-sk6812

This is a satellite driver for [mauzr](https://mauzr.eqrx.net) to drive SK6812 LEDs. Mauzr is normally running
on SOCs that are not realtime, which is a problem since the LEDs need to be set with nanosecond precision.
While it is still possible to do that on SOCs the correct way to do so varies dramatically depending on chip and
distribution.

To circument that a esp32 application has been written that reads color channels via UART and sets an SK6812 pixel
strip after all channels have been transmitted. This also works with WS2812 and all the other 1-Wire LEDs

## Licence
This project is released under GNU Affero General Public License v3.0, see
LICENCE file in this repo for more info.
