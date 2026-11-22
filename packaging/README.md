# Building and installing on a CardputerZero

1. Turn on SSH on the device (Settings, or when writing the card with the M5Stack imager).
2. Check the hardware (reads only, changes nothing):

   ```bash
   ssh user@host 'sh -s' < tools/probe.sh
   ```

3. Build the package (needs Docker):

   ```bash
   packaging/build-deb.sh
   ```

4. Install it:

   ```bash
   packaging/deploy.sh user@host
   ```

BattleZero then appears in the launcher. To run it from an SSH shell instead, with a
frame-rate counter:

```bash
BATTLEZERO_FPS=1 battlezero
```

## Settings read from the environment

| Variable | Meaning |
|---|---|
| `BATTLEZERO_FB` | framebuffer device, default `/dev/fb0` |
| `BATTLEZERO_KEYS` | one evdev device to use instead of searching `/dev/input` |
| `BATTLEZERO_IMU` | IIO device directory instead of searching for the BMI270 |
| `BATTLEZERO_AUDIO` | play straight to this ALSA device instead of the system sound server |
| `BATTLEZERO_FPS` | show a frame-rate counter |

The tilt axis and direction chosen on the calibration screen are kept in
`~/.config/battlezero/tilt.conf`; the high score in `~/.local/share/battlezero/highscore`.
