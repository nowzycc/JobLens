#!/usr/bin/env bash
# usage: collect_so.sh <executable> <output_dir>
set -e
EXE=$(realpath "$1")
OUT=$(realpath "$2")
SYSTEM_LIBS=(
    # 白名单：不打包的系统库
    /lib64/ld-linux
    libc.so.6
    libm.so.6
    libdl.so.2
    libpthread.so.0
    librt.so.1
    libresolv.so.2
    linux-vdso.so.1
)

# 递归拷贝函数
copy_deps(){
    local file="$1"
    # 使用增强解析逻辑
    for lib in $(ldd "$file" 2>/dev/null | awk '{if ($2 == "=>" && $3 ~ /^\//) print $3}' | sort -u); do
        local name=$(basename "$lib")
        # 跳过白名单
        for skip in "${SYSTEM_LIBS[@]}"; do
            [[ "$lib" == *"$skip"* ]] && continue 2
        done
        # 已拷贝过则跳过
        [[ -f "$OUT/$name" ]] && continue
        cp -Lv "$lib" "$OUT/" 2>/dev/null || true
        # 继续递归该 .so 本身
        copy_deps "$OUT/$name"
    done
}

copy_deps "$EXE"