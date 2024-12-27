#!/usr/bin/env python3
# This file is part of the OpenMV project.
#
# Copyright (c) 2013-2024 Ibrahim Abdelkader <iabdalkader@openmv.io>
# Copyright (c) 2013-2024 Kwabena W. Agyeman <kwagyeman@openmv.io>
#
# This work is licensed under the MIT license, see the file LICENSE for details.
#
# This script creates a romfs image.

import sys
import os
import json
import argparse
import struct
from tflite2c import vela_compile
from haar2c import cascade_binary_universal

CG = '\033[92m'
CR = '\033[91m'
CB = '\033[94m'
CN = '\033[0m'

ROMFS_HEADER = 0x14a6b1
ROMFS_HEADER_ALIGN = 16
ROMFS_FILEREC_ALIGN = 8
ROMFS_RECORD_KIND_PADDING = 1
ROMFS_RECORD_KIND_DATA = 2
ROMFS_RECORD_KIND_FILE = 5


def encode_vint(value):
    encoded = [value & 0x7F]
    value >>= 7
    while value != 0:
        encoded.insert(0, 0x80 | (value & 0x7F))
        value >>= 7
    return bytes(encoded)

def encode_record(kind, payload, align=0, offset=0, padding=0):
    if align:
        # Set offset to where the payload starts to align the payload.
        offset += len(encode_vint(kind) + encode_vint(len(payload)))
        padding = ((offset + (align - 1)) & ~(align - 1)) - offset
    kind = encode_vint(kind) if kind else b""
    return kind + (b"\x80" * padding) + encode_vint(len(payload)) + payload

def encode_file(name, data, align, offset):
    # file record: <FILE_KIND>, record_length, name_length, name, [<DATA_KIND>, data_length, data]
    # Note that the file record's length is rounded up to a fixed size, by aligning it
    # from offset 0, to ensure that the record length remains unchanged after alignment.
    name_rec = encode_record(0, name)
    # Data record offset = file offset + 1 (type) + (align - 1) + name record length.
    offset = offset + 1 + (ROMFS_FILEREC_ALIGN - 1) + len(name_rec)
    # Align the data record's payload.
    data_rec = encode_record(ROMFS_RECORD_KIND_DATA, data, align, offset)
    # Align the file record's payload.
    file_rec = encode_record(ROMFS_RECORD_KIND_FILE, name_rec + data_rec, ROMFS_FILEREC_ALIGN)
    return file_rec

def romfs_build(romfs_cfg, p, args):
    romfs_cfg = romfs_cfg[p]

    # To replace variables in the path
    variables = {
        "TOP" : args.top_dir,
        "BUILD" : args.build_dir
    }

    # Build/convert files if needed.
    index = 0
    while index < len(romfs_cfg["entries"]):
        entry = romfs_cfg["entries"][index]
        if not entry.get("enabled", True):
            romfs_cfg["entries"].pop(index)
            continue
        file_path = entry['path'].format(**variables)
        file_name = os.path.basename(os.path.splitext(file_path)[0])
        file_size = "%.2f KiB"%(os.path.getsize(file_path) / 1024)
        if entry['type'] == 'haar':
            # Convert Haar cascade
            output_path = os.path.join(args.build_dir, file_name)
            cascade_binary_universal(file_path, entry["stages"], output_path)
            file_path = os.path.join(args.build_dir, file_name + ".cascade")
        elif entry['type'] == 'tflite':
            _file_path = file_path  # to get labels
            if args.vela_args:
                # Compile the model using Vela.
                vela_args = args.vela_args + " --optimise " + entry["optimize"]
                vela_compile(file_path, args.build_dir, vela_args.split())
                file_path = os.path.join(args.build_dir, file_name + ".tflite")
            # If tflite has a labels file add it as a new entry
            labels_path = os.path.splitext(_file_path)[0] + ".txt"
            if entry['type'] == 'tflite' and os.path.exists(labels_path):
                romfs_cfg["entries"].insert(index + 1, { "type" : "txt", "path" : labels_path })

        index += 1
        entry['path'] = file_path

    # Build romfs image.
    romfs_data = bytearray()
    romfs_offset = ROMFS_HEADER_ALIGN
    romfs_size = int(romfs_cfg["size"], 16)

    for entry in romfs_cfg["entries"]:
        file_path = entry['path']
        file_name = os.path.basename(file_path)
        file_size = os.path.getsize(file_path)
        file_align = entry.get("alignment", 4)
        with open(file_path, "rb") as file:
            file_data = file.read()
        record = encode_file(bytes(file_name, "ascii"), file_data, file_align, romfs_offset)
        romfs_offset += len(record)
        romfs_data += record

    # Write the romfs image.
    with open(os.path.join(args.out_dir, f"romfs{p}.img"), "wb") as romfs_file:
        # Pad the ROMFS header to ensure a fixed offset from the start of the file.
        romfs_data = encode_record(ROMFS_HEADER, romfs_data, ROMFS_HEADER_ALIGN)
        if len(romfs_data) > romfs_size:
            print(f"{CR}romfs partition overflow "
                  f"{CR}{len(romfs_data)/1024:.1f}KiB / {CR}{romfs_size/1024:.1f}KiB "
                  f"({CR}{(len(romfs_data) / romfs_size) * 100:.1f}%){CN}")
            print(
                  f"Partition size: {romfs_size/1024:.1f} KiB"
                  f"ROMFS Size: {len(romfs_data)/1024:.1f} KiB{CN}")
            raise Exception("{CR}romfs partition overflow{CN}")
        romfs_file.write(romfs_data)

    print(f"{CB}romfs image: {CR}\"romfs{p}.img\" {CB}usage: "
          f"{CR}{len(romfs_data)/1024:.1f}KiB / {CR}{romfs_size/1024:.1f}KiB "
          f"({CR}{(len(romfs_data) / romfs_size) * 100:.1f}%){CN}")
    for entry in romfs_cfg["entries"]:
        file_path = entry['path']
        file_name = os.path.basename(file_path)
        file_size = os.path.getsize(file_path)
        file_align = entry.get("alignment", 4)
        print(f" {CB}-size: {CR}{file_size:<8} {CB}alignment: {CR}{file_align:<4} {CB}path: {CG}/rom/{file_name}{CN}");
    print("")

def main():
    # Set up argument parser
    parser = argparse.ArgumentParser(description='Create a romfs image from json file.')
    parser.add_argument('--config', action = 'store', help = 'Input romfs json config file.', required=True)
    parser.add_argument('--top-dir', action = 'store', help='Top directory', required=True)
    parser.add_argument('--out-dir', action = 'store', help='Output directory', required=True)
    parser.add_argument('--build-dir', action = 'store', help='Build directory', required=True)
    parser.add_argument('--vela-args', action = 'store', help='Vela compiler args', default='')
    parser.add_argument('--partition', action = 'store', help = 'romfs partition to build. Default=all.', default=None)

    # Parse arguments
    args = parser.parse_args()

    # Load romfs config from the JSON file
    try:
        with open(args.config, 'r') as file:
            romfs_cfg = json.loads(file.read())
    except Exception as e:
        print(f"Error: Unable to read the file '{path}'. {str(e)}")
        sys.exit(1)
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON format in '{path}'. {str(e)}")
        sys.exit(1)
    
    if args.partition is None:
        for p in romfs_cfg:
            romfs_build(romfs_cfg, p, args)
    else:
        romfs_build(romfs_cfg, args.partition, args)

if __name__ == '__main__':
    main()
