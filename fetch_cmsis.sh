#!/bin/bash
# Usage: ./fetch_cmsis.sh <project_dir> [mcu_variant]
# Example: ./fetch_cmsis.sh my-project stm32f446xx
#
# Copies the minimal CMSIS headers needed for STM32F4 into <project_dir>/cmsis/
# Default MCU variant is stm32f446xx

set -e

PROJECT_DIR="${1:?Usage: $0 <project_dir> [mcu_variant]}"
MCU="${2:-stm32f446xx}"
CMSIS_SRC="$(dirname "$0")/CMSIS"
DEST="${PROJECT_DIR:-"."}/CMSIS"

# Validate source
if [ ! -d "$CMSIS_SRC" ]; then
    echo "Error: CMSIS source not found at $CMSIS_SRC"
    exit 1
fi

# Validate MCU header exists
if [ ! -f "$CMSIS_SRC/device/Include/${MCU}.h" ]; then
    echo "Error: No header for MCU '${MCU}' in $CMSIS_SRC/device/Include/"
    echo "Available:"
    ls "$CMSIS_SRC/device/Include/"stm32f4*.h | xargs -n1 basename
    exit 1
fi

mkdir -p "$DEST"

# --- Cortex-M4 core headers (5 files) ---
CORE_FILES=(
    core_cm4.h
    cmsis_compiler.h
    cmsis_gcc.h
    cmsis_version.h
    mpu_armv7.h
)
for f in "${CORE_FILES[@]}"; do
    cp "$CMSIS_SRC/core/$f" "$DEST/"
done

# --- STM32F4 device headers (3 files) ---
cp "$CMSIS_SRC/device/Include/stm32f4xx.h"          "$DEST/"
cp "$CMSIS_SRC/device/Include/${MCU}.h"              "$DEST/"
cp "$CMSIS_SRC/device/Include/system_stm32f4xx.h"   "$DEST/"

echo "Done. Copied 8 CMSIS headers to $DEST/"
echo "  Core : ${CORE_FILES[*]}"
echo "  Device: stm32f4xx.h  ${MCU}.h  system_stm32f4xx.h"
echo ""
echo "Add to your Makefile:"
echo "  CFLAGS += -I./cmsis -D$(echo $MCU | tr '[:lower:]' '[:upper:]')"
