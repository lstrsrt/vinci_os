#!/bin/sh
# Builds a bootable UEFI disk image from vdisk/ (next to this script) and
# installs it into a VirtualBox VM, or writes it to a standalone file.
#
# Usage:
#   ./make_vbox_disk.sh [VM name]       update the VM's disk (default: VinciOS)
#   ./make_vbox_disk.sh -o out.vdi      only write an image file (.vdi/.vmdk/.vhd/.img)
#
# The disk gets a GPT with one FAT32 EFI system partition holding the contents
# of vdisk/. If the VM already has a disk, it is replaced but keeps its UUID,
# so the VM config doesn't change. If it has none, one is created and attached.
#
# Needs: sfdisk (util-linux), mkfs.fat (dosfstools), mtools, VBoxManage.

set -eu

die() { echo "error: $*" >&2; exit 1; }

usage() {
    sed -n '2,13s/^# \{0,1\}//p' "$0"
    exit "${1:-0}"
}

VM=VinciOS
OUT=
while [ $# -gt 0 ]; do
    case "$1" in
        -h|--help) usage ;;
        -o) [ $# -ge 2 ] || usage 1; OUT=$2; shift 2 ;;
        -*) usage 1 ;;
        *) VM=$1; shift ;;
    esac
done

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
SRC="$SCRIPT_DIR/vdisk"

PATH="$PATH:/sbin:/usr/sbin"  # mkfs.fat and sfdisk live here on some distros
command -v sfdisk >/dev/null || die "sfdisk not found (install util-linux)"
command -v mkfs.fat >/dev/null || die "mkfs.fat not found (install dosfstools)"
command -v mcopy >/dev/null || die "mcopy not found (install mtools)"
[ -f "$SRC/EFI/BOOT/BOOTX64.EFI" ] || die "$SRC/EFI/BOOT/BOOTX64.EFI missing, build the project first"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT INT TERM

echo "Building disk image from $SRC"

# Partition size: contents plus slack, at least 64 MiB (FAT32 needs ~33 MiB).
CONTENT_MB=$(du -sm "$SRC" | cut -f1)
PART_MB=$((CONTENT_MB * 2 + 16))
[ "$PART_MB" -ge 64 ] || PART_MB=64

# Make the FAT32 filesystem in its own file and copy the files in.
# 512-byte clusters keep small partitions above FAT32's minimum cluster count.
truncate -s "${PART_MB}M" "$TMP/part.img"
mkfs.fat -F 32 -s 1 -S 512 -n VINCIOS "$TMP/part.img" >/dev/null
export MTOOLS_SKIP_CHECK=1
for f in "$SRC"/* "$SRC"/.[!.]*; do
    [ -e "$f" ] || continue
    mcopy -s -m -Q -i "$TMP/part.img" "$f" ::/
done

# Disk: 1 MiB for the GPT, the partition, 1 MiB for the backup GPT.
truncate -s "$((PART_MB + 2))M" "$TMP/disk.raw"
printf 'label: gpt\nstart=1MiB, size=%sMiB, type=uefi, name="EFI system partition"\n' "$PART_MB" \
    | sfdisk -q "$TMP/disk.raw"
dd if="$TMP/part.img" of="$TMP/disk.raw" bs=1M seek=1 conv=notrunc status=none

format_for() {
    case "$1" in
        *.vdi|*.VDI) echo VDI ;;
        *.vmdk|*.VMDK) echo VMDK ;;
        *.vhd|*.VHD) echo VHD ;;
        *) echo RAW ;;
    esac
}

# Standalone mode: just write the file.
if [ -n "$OUT" ]; then
    FMT=$(format_for "$OUT")
    rm -f "$OUT"
    if [ "$FMT" = RAW ]; then
        mv "$TMP/disk.raw" "$OUT"
    else
        command -v VBoxManage >/dev/null || die "VBoxManage not found (install VirtualBox)"
        VBoxManage convertfromraw "$TMP/disk.raw" "$OUT" --format "$FMT" >/dev/null
    fi
    echo "Wrote $OUT"
    exit 0
fi

# VM mode.
command -v VBoxManage >/dev/null || die "VBoxManage not found (install VirtualBox)"
INFO=$(VBoxManage showvminfo "$VM" --machinereadable 2>/dev/null) \
    || die "no VirtualBox VM named '$VM' (see: VBoxManage list vms)"

# Reads a value out of the machine-readable VM info.
info() { printf '%s\n' "$INFO" | sed -n "s/^\"\{0,1\}$1\"\{0,1\}=\"\{0,1\}\([^\"]*\)\"\{0,1\}\$/\1/p" | head -n 1; }

[ "$(info VMState)" = poweroff ] || [ "$(info VMState)" = aborted ] \
    || die "VM '$VM' is $(info VMState), power it off first"
[ "$(info firmware)" = EFI ] \
    || echo "warning: VM '$VM' doesn't use EFI firmware; enable it under System > Motherboard" >&2
# VirtualBox's EFI firmware triple faults (Guru Meditation) with too little RAM.
[ "$(info memory)" -ge 128 ] \
    || echo "warning: VM '$VM' has $(info memory) MB RAM; EFI needs at least 128 (VBoxManage modifyvm \"$VM\" --memory 256)" >&2

# Find the first attached hard disk (an entry like "SATA-ImageUUID-0-0"="...").
KEY=$(printf '%s\n' "$INFO" | sed -n 's/^"\(.*\)-ImageUUID-\([0-9]*-[0-9]*\)"=.*/\1-\2/p' | while read -r k; do
    path=$(info "$k")
    case "$path" in *.vdi|*.VDI|*.vmdk|*.VMDK|*.vhd|*.VHD) echo "$k"; break ;; esac
done)

if [ -n "$KEY" ]; then
    DISK=$(info "$KEY")
    UUID=$(info "$(echo "$KEY" | sed 's/-\([0-9]*-[0-9]*\)$/-ImageUUID-\1/')")
    [ -z "$(info SnapshotName)" ] || die "VM '$VM' has snapshots; delete them first or use -o"
    FMT=$(format_for "$DISK")
    echo "Replacing $DISK"
    BACKUP="$DISK.bak"
    mv "$DISK" "$BACKUP"
    if VBoxManage convertfromraw "$TMP/disk.raw" "$DISK" --format "$FMT" --uuid "$UUID" >/dev/null; then
        rm -f "$BACKUP"
    else
        mv "$BACKUP" "$DISK"
        die "VBoxManage convertfromraw failed, old disk restored"
    fi
else
    # No disk yet: create one next to the .vbox file and attach it.
    DISK="$(dirname "$(info CfgFile)")/$VM.vdi"
    [ ! -e "$DISK" ] || die "$DISK exists but isn't attached to '$VM'; attach or remove it"
    echo "Creating $DISK"
    VBoxManage convertfromraw "$TMP/disk.raw" "$DISK" --format VDI >/dev/null
    CTL=$(info storagecontrollername0)
    if [ -z "$CTL" ]; then
        CTL=SATA
        VBoxManage storagectl "$VM" --name "$CTL" --add sata --controller IntelAhci
    fi
    VBoxManage storageattach "$VM" --storagectl "$CTL" --port 0 --device 0 --type hdd --medium "$DISK"
fi

echo "Done. Start the VM with: VBoxManage startvm \"$VM\""
