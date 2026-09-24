#!/bin/sh
# TrimUI Smart Pro stock and Knulli. POSIX sh, Unix LF.
# Do not change framebuffer.
#
# 32-bit Mass Effect Infiltrator talks to a 64-bit PowerVR presenter.
# glbridge is first on --library-path so dlopen("libmali.so.1") hits
# the fake 32-bit client. Presenter sockets stay the Dead Space names.

if [ -f /mnt/SDCARD/System/etc/ex_config ]; then
  . /mnt/SDCARD/System/etc/ex_config
fi

export PATH="/mnt/SDCARD/System/bin:/usr/bin:/usr/sbin:/bin:/sbin:${PATH:-}"

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
if [ -x "$HERE/masseffect/masseffect" ]; then
  GAMEDIR="$HERE/masseffect"
elif [ -x /mnt/SDCARD/Data/ports/masseffect/masseffect ]; then
  GAMEDIR="/mnt/SDCARD/Data/ports/masseffect"
elif [ -x /userdata/roms/ports/masseffect/masseffect ]; then
  GAMEDIR="/userdata/roms/ports/masseffect"
else
  GAMEDIR="$HERE/masseffect"
fi

SYS="$GAMEDIR/armhf"
LD="$SYS/lib/ld-linux-armhf.so.3"
LIB="$GAMEDIR/glbridge:$GAMEDIR/host-libs:$GAMEDIR/libs.armhf:$SYS/lib/arm-linux-gnueabihf:$SYS/lib"
LOGDIR="$GAMEDIR/logs"
LOG="$LOGDIR/masseffect.log"
PRES=0
KB=0
GCDB=""
PVR=""

if [ -d /mnt/SDCARD/Apps/PortMaster/PortMaster ]; then
  PM="/mnt/SDCARD/Apps/PortMaster/PortMaster"
elif [ -n "${controlfolder:-}" ]; then
  PM="$controlfolder"
elif [ -d /userdata/system/.local/share/PortMaster ]; then
  PM="/userdata/system/.local/share/PortMaster"
else
  PM=""
fi

for d in /usr/trimui/lib /usr/lib /usr/lib/aarch64-linux-gnu /lib; do
  if [ -d "$d" ]; then
    if [ -n "$PVR" ]; then
      PVR="$PVR:$d"
    else
      PVR="$d"
    fi
  fi
done

for f in \
  ${PM:+$PM/gamecontrollerdb.txt} \
  /userdata/system/.local/share/PortMaster/gamecontrollerdb.txt \
  /usr/share/sdl/gamecontrollerdb.txt \
  /usr/share/gamecontrollerdb.txt
do
  if [ -f "$f" ]; then
    GCDB="$f"
    break
  fi
done

mkdir -p "$LOGDIR" "$GAMEDIR/var" "$GAMEDIR/saves" /tmp || exit 1

if [ -f "$LOG" ]; then
  mv -f "$LOG" "$LOG.1" 2>/dev/null || true
fi

echo "===== masseffect tsp start =====" > "$LOG"
date >> "$LOG" 2>/dev/null
echo "gamedir=$GAMEDIR" >> "$LOG"
echo "uname=$(uname -a)" >> "$LOG"
echo "id=$(id)" >> "$LOG"

cd "$GAMEDIR" || {
  echo "cannot cd $GAMEDIR" >> "$LOG"
  exit 1
}

rm -f /tmp/deadspace.present.ready /tmp/ds-glbridge.sock /tmp/dsgl-xport /tmp/deadspace.frame
chmod a+x "$GAMEDIR/masseffect" "$GAMEDIR/masseffect_present" "$LD" 2>/dev/null || true
chmod a+rw /dev/dri/card0 /dev/dri/renderD128 /dev/fb0 2>/dev/null || true

c=0
while [ "$c" -lt 4 ]; do
  echo 1 >/sys/devices/system/cpu/cpu$c/online 2>/dev/null || true
  if [ -f /sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor ]; then
    echo performance >/sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor
    if [ -f /sys/devices/system/cpu/cpu$c/cpufreq/cpuinfo_max_freq ]; then
      freq=$(cat /sys/devices/system/cpu/cpu$c/cpufreq/cpuinfo_max_freq)
      echo "$freq" >/sys/devices/system/cpu/cpu$c/cpufreq/scaling_min_freq
      echo "$freq" >/sys/devices/system/cpu/cpu$c/cpufreq/scaling_max_freq
    fi
  fi
  c=$((c + 1))
done
for g in /sys/class/devfreq/*/governor; do
  if [ -f "$g" ]; then
    echo performance >"$g" 2>/dev/null || true
  fi
done

GAME_SO="$GAMEDIR/lib/armeabi/libMassEffect.so"
if [ ! -f "$GAME_SO" ] || [ ! -f "$GAMEDIR/assets/EAMCore.ini" ] \
   || [ ! -d "$GAMEDIR/assets/published" ]; then
  echo "missing Mass Effect Infiltrator v1.0.58 data" >> "$LOG"
  echo "need: $GAME_SO" >> "$LOG"
  echo "need: $GAMEDIR/assets/EAMCore.ini" >> "$LOG"
  echo "need: $GAMEDIR/assets/published/" >> "$LOG"
  echo "===== masseffect tsp end =====" >> "$LOG"
  sync
  exit 1
fi

EXPECTED_SIZE=16952541
GAME_SIZE=$(wc -c < "$GAME_SO")
GAME_SIZE=$(echo $GAME_SIZE)
if [ -n "$GAME_SIZE" ] && [ "$GAME_SIZE" != "$EXPECTED_SIZE" ]; then
  echo "unsupported native library size=$GAME_SIZE expected=$EXPECTED_SIZE" >> "$LOG"
  echo "===== masseffect tsp end =====" >> "$LOG"
  sync
  exit 1
fi

if [ ! -f "$LD" ] || [ ! -f "$GAMEDIR/masseffect" ]; then
  echo "missing armhf ld.so or masseffect loader" >> "$LOG"
  echo "===== masseffect tsp end =====" >> "$LOG"
  sync
  exit 1
fi

if [ ! -f "$GAMEDIR/glbridge/libmali.so.1" ] || [ ! -x "$GAMEDIR/masseffect_present" ]; then
  echo "missing GLES1 glbridge (libmali.so.1 / masseffect_present)" >> "$LOG"
  echo "===== masseffect tsp end =====" >> "$LOG"
  sync
  exit 1
fi

STRINGS_DIR="$GAMEDIR/assets/published/strings"
ENG_BIN="$STRINGS_DIR/ENG_US/masseffect.bin"
RUS_BIN="$STRINGS_DIR/RUS_RU/masseffect.bin"
if [ -f "$ENG_BIN" ] && [ -f "$RUS_BIN" ]; then
  if ! grep -qa "Mattock" "$ENG_BIN" && grep -qa "Mattock" "$RUS_BIN"; then
    SWAP_TMP="$STRINGS_DIR/.masseffect.bin.swap"
    if mv "$ENG_BIN" "$SWAP_TMP" && mv "$RUS_BIN" "$ENG_BIN" \
       && mv "$SWAP_TMP" "$RUS_BIN"; then
      echo "Language: swapped reversed ENG_US/RUS_RU string tables" >> "$LOG"
    else
      rm -f "$SWAP_TMP"
      echo "Language: could not swap reversed string tables" >> "$LOG"
    fi
  fi
fi

SCALE="${MASSEFFECT_SCALE:-stretch}"
if [ -x "$GAMEDIR/masseffect_present" ]; then
  echo "----- gles1 present server -----" >> "$LOG"
  echo "glbridge=$(ls -l "$GAMEDIR/glbridge/libmali.so.1" 2>/dev/null)" >> "$LOG"
  (
    unset LD_PRELOAD
    unset SDL_VIDEODRIVER
    unset LIBGL_ALWAYS_SOFTWARE
    unset GALLIUM_DRIVER
    unset MESA_LOADER_DRIVER_OVERRIDE
    unset LIBGL_DRIVERS_PATH
    unset __EGL_VENDOR_LIBRARY_FILENAMES
    unset SDL_VIDEO_EGL_DRIVER
    export LD_LIBRARY_PATH="$PVR"
    export SDL_VIDEO_GL_DRIVER=libGLESv2.so
    export SDL_OPENGL_ES_DRIVER=1
    if [ -n "$GCDB" ]; then
      export SDL_GAMECONTROLLERCONFIG_FILE="$GCDB"
    fi
    export XDG_RUNTIME_DIR=/tmp
    export TMPDIR=/tmp
    export TSPGL_WIDTH="${TSPGL_WIDTH:-640}"
    export TSPGL_HEIGHT="${TSPGL_HEIGHT:-480}"
    export TSPGL_PRESENT="$SCALE"
    if command -v setsid >/dev/null 2>&1; then
      exec setsid "$GAMEDIR/masseffect_present"
    else
      exec "$GAMEDIR/masseffect_present"
    fi
  ) >> "$LOG" 2>&1 &
  PRES=$!
  n=0
  while [ "$n" -lt 15 ]; do
    if [ -f /tmp/deadspace.present.ready ]; then
      break
    fi
    n=$((n + 1))
    sleep 1
  done
  echo "present_ready=$n pid=$PRES" >> "$LOG"
fi

export PORT_32BIT=Y
export XDG_RUNTIME_DIR=/tmp
export TMPDIR=/tmp
export SDL_VIDEODRIVER=offscreen
PULSE_SOCK=""
for s in /run/user/0/pulse/native /var/run/pulse/native /run/pulse/native; do
  if [ -S "$s" ]; then
    PULSE_SOCK=$s
    break
  fi
done
if [ -z "$PULSE_SOCK" ]; then
  for d in /run/user/*; do
    if [ -S "$d/pulse/native" ]; then
      PULSE_SOCK=$d/pulse/native
      break
    fi
  done
fi
if [ -n "$PULSE_SOCK" ]; then
  export SDL_AUDIODRIVER=pulse
  export PULSE_SERVER="unix:$PULSE_SOCK"
  export PULSE_LATENCY_MSEC="${PULSE_LATENCY_MSEC:-80}"
  for c in /var/run/pulse/.config/pulse/cookie /root/.config/pulse/cookie; do
    if [ -f "$c" ]; then
      export PULSE_COOKIE="$c"
      break
    fi
  done
else
  export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
fi
echo "audio_driver=$SDL_AUDIODRIVER pulse=$PULSE_SOCK" >> "$LOG"
export SDL_VIDEO_GL_DRIVER=libGLESv2.so.2
export SDL_VIDEO_EGL_DRIVER=libEGL.so.1
export SDL_OPENGL_ES_DRIVER=1
if [ -n "$GCDB" ]; then
  export SDL_GAMECONTROLLERCONFIG_FILE="$GCDB"
fi
export SDL_NO_SIGNAL_HANDLERS=1
export SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1
export MASSEFFECT_SCALE="$SCALE"
export MASSEFFECT_FACE_LAYOUT="${MASSEFFECT_FACE_LAYOUT:-nintendo}"
export MASSEFFECT_NO_CURSOR="${MASSEFFECT_NO_CURSOR:-1}"
export MASSEFFECT_PANEL_W="${MASSEFFECT_PANEL_W:-640}"
export MASSEFFECT_PANEL_H="${MASSEFFECT_PANEL_H:-480}"
export MASSEFFECT_SCREEN_W="${MASSEFFECT_SCREEN_W:-640}"
export MASSEFFECT_SCREEN_H="${MASSEFFECT_SCREEN_H:-480}"
export MASSEFFECT_FORCE_SOFTWARE_TEXTURE_DECODE="${MASSEFFECT_FORCE_SOFTWARE_TEXTURE_DECODE:-1}"
export MASSEFFECT_DXT_MAX_DIM="${MASSEFFECT_DXT_MAX_DIM:-1024}"
if [ -d /userdata/roms ]; then
  export MASSEFFECT_SWAP_L2R2=1
  export SDL_GAMECONTROLLERCONFIG="03000000000000000000000002000000,TRIMUI Smart Pro Controller,platform:Linux,b:b0,a:b1,dpdown:h0.4,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:b6,dpleft:h0.8,rightshoulder:b5,leftshoulder:b4,righttrigger:b7,dpright:h0.2,back:b8,start:b9,dpup:h0.1,y:b2,x:b3,guide:b10,"
fi
echo "DXT_MAX_DIM=$MASSEFFECT_DXT_MAX_DIM" >> "$LOG"
echo "gcdb=$GCDB" >> "$LOG"
awk '/MemTotal|MemAvailable|MemFree/{print}' /proc/meminfo >> "$LOG" 2>/dev/null || true
export MASSEFFECT_SAVEDIR="${MASSEFFECT_SAVEDIR:-$GAMEDIR/saves}"
export LOADER_TRACE="${LOADER_TRACE:-0}"
export MALLOC_ARENA_MAX=2
unset LIBGL_ALWAYS_SOFTWARE
unset GALLIUM_DRIVER
unset MESA_LOADER_DRIVER_OVERRIDE
unset LIBGL_DRIVERS_PATH
unset __EGL_VENDOR_LIBRARY_FILENAMES
unset EGL_PLATFORM
unset LD_PRELOAD

if [ -n "$PM" ] && [ -x "$PM/gptokeyb" ]; then
  "$PM/gptokeyb" "masseffect" -c "$GAMEDIR/masseffect.gptk" >> "$LOG" 2>&1 &
  KB=$!
  echo "gptokeyb pid=$KB" >> "$LOG"
fi

STICK=""
if [ -f "$GAMEDIR/glbridge/libsticksnap.so" ]; then
  STICK="$GAMEDIR/glbridge/libsticksnap.so"
  echo "sticksnap=$STICK" >> "$LOG"
fi

echo "ld=$LD" >> "$LOG"
echo "lib=$LIB" >> "$LOG"
echo "----- masseffect loader -----" >> "$LOG"
if [ -n "$STICK" ]; then
  LD_PRELOAD="$STICK" "$LD" --library-path "$LIB" "$GAMEDIR/masseffect" "$GAMEDIR" >> "$LOG" 2>&1
else
  "$LD" --library-path "$LIB" "$GAMEDIR/masseffect" "$GAMEDIR" >> "$LOG" 2>&1
fi
RC=$?

if [ "$KB" -ne 0 ]; then
  kill "$KB" 2>/dev/null || true
fi
if [ "$PRES" -ne 0 ]; then
  kill "$PRES" 2>/dev/null || true
  wait "$PRES" 2>/dev/null || true
fi
rm -f /tmp/deadspace.present.ready /tmp/ds-glbridge.sock /tmp/dsgl-xport /tmp/deadspace.frame
echo "exit_code=$RC" >> "$LOG"
echo "===== masseffect tsp end =====" >> "$LOG"
sync
exit $RC
