#!/bin/sh
set -e

mod_list="led_drv ap3216c_drv dht11_drv sr501_drv"

for name in $mod_list; do
    ko="./${name}.ko"
    if [ -f "$ko" ]; then
        echo "[INFO] loading $ko"
        insmod "$ko" || echo "[WARN] $ko already inserted?"
    else
        echo "[ERROR] missing $ko"
    fi
done

./My_Project
