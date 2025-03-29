#!/usr/bin/env bash
set -eou pipefail

pio run -t upload -e "$1"
pio device monitor -e "$1"  --quiet

