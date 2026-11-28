#!/bin/sh
set -eu

cd "$(dirname "$0")/.."
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT
cc=${CC:-cc}
firmware=src/firmware
crypto=$firmware/services/crypto
cmsis=$firmware/Drivers/CMSIS

"$cc" -std=c11 -O2 -Wall -Wextra -Werror \
    -I "$firmware/Core/Inc" -I "$crypto" -I "$firmware/services/radio" \
    tests/test_crypto.c "$firmware/Core/Src/aes.c" \
    "$crypto/aes_gcm.c" "$crypto/x25519.c" "$crypto/kdf.c" \
    "$crypto/confirm.c" "$crypto/crypto.c" "$crypto/rng.c" -o "$build_dir/test_crypto"

"$cc" -std=c11 -O2 -Wall -Wextra -Werror -DSTM32F042x6 \
    -I "$firmware/Core/Inc" \
    -isystem "$cmsis/Include" -isystem "$cmsis/Device/ST/STM32F0xx/Include" \
    tests/test_entropy_health.c "$firmware/Core/Src/aes.c" \
    -o "$build_dir/test_entropy_health"

"$build_dir/test_crypto"
"$build_dir/test_entropy_health"
