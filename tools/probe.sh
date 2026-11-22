#!/bin/sh
# Read-only hardware check for a CardputerZero. Changes nothing on the device.
#   ssh user@host 'sh -s' < tools/probe.sh
echo "== system"; uname -a; grep -E '^(PRETTY_NAME|VERSION_CODENAME)=' /etc/os-release
echo "user: $(id)"
ldd --version 2>/dev/null | head -n 1

echo "== framebuffer"
for fb in /sys/class/graphics/fb*; do
  [ -d "$fb" ] || continue
  echo "$fb: name=$(cat "$fb/name" 2>/dev/null) size=$(cat "$fb/virtual_size" 2>/dev/null) bpp=$(cat "$fb/bits_per_pixel" 2>/dev/null) stride=$(cat "$fb/stride" 2>/dev/null)"
done
ls -l /dev/fb* /dev/dri/* 2>/dev/null
[ -w /dev/fb0 ] && echo "fb0 writable by this user: yes" || echo "fb0 writable by this user: NO"

echo "== input devices"
grep -E '^(N:|H:)' /proc/bus/input/devices
ls -l /dev/input/ /dev/input/by-path/ 2>/dev/null
for e in /dev/input/event*; do [ -r "$e" ] && echo "$e readable: yes" || echo "$e readable: NO"; done

echo "== motion sensor (IIO)"
for d in /sys/bus/iio/devices/iio:device*; do
  [ -d "$d" ] || continue
  echo "$d: name=$(cat "$d/name" 2>/dev/null)"
  ls "$d" | grep -E 'in_(accel|anglvel|magn)_[xyz]_raw|scale|sampling_frequency|mount_matrix' | tr '\n' ' '; echo
  [ -r "$d/in_accel_mount_matrix" ] && echo "mount matrix: $(cat "$d/in_accel_mount_matrix")"
done
echo "-- five accelerometer readings, one per second (x y z). Lying flat, then tilt it:"
for d in /sys/bus/iio/devices/iio:device*; do
  [ -r "$d/in_accel_x_raw" ] || continue
  i=0
  while [ $i -lt 5 ]; do
    echo "$(cat "$d/in_accel_x_raw") $(cat "$d/in_accel_y_raw") $(cat "$d/in_accel_z_raw")"
    i=$((i + 1)); sleep 1
  done
  start=$(date +%s%N); n=0
  while [ $n -lt 50 ]; do cat "$d/in_accel_x_raw" >/dev/null; n=$((n + 1)); done
  end=$(date +%s%N)
  echo "50 reads took $(( (end - start) / 1000000 )) ms"
done

echo "== launcher"
ls /usr/share/APPLaunch/applications/ 2>/dev/null | head -n 20
ps -eo user,comm,args 2>/dev/null | grep -iE 'launch|lvgl' | grep -v grep | head -n 5
echo "== build tools on the device"; command -v g++ cmake make || echo "none"
