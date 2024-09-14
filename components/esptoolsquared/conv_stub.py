#!/usr/bin/env python3

import json, os, argparse, base64

parser = argparse.ArgumentParser(
    usage="Converts esptool.py flasher stub JSON files into esptoolsquared C constants"
)
parser.add_argument("infile", action="store", help="Input JSON file")
parser.add_argument("--id",   action="store", help="ROM ID prefix [flashstub]", default="flashstub")
args = parser.parse_args()

with open(args.infile, "r") as fd:
    obj = json.load(fd)

def mkrom(id: str, data: bytes):
    print(f"    .{id} = (uint8_t const[]){{")
    print(f"        {", ".join(map(lambda x: f"0x{x:02X}", data))}")
    print( "    },")
    print(f"    .{id}_len = {len(data)},")

def mkconst(id: str, data: int):
    print(f"    .{id} = 0x{data:08X},")

print("// WARNING: This is a generated file, do not edit it!")
print("// clang-format off")
print(f"// Generated from {os.path.relpath(args.infile)}")
print("")
print("#include \"chips.h\"")
print("")
print(f"et2_stub_t const {args.id} = {{")
mkrom("text", base64.b64decode(obj['text']))
mkrom("data", base64.b64decode(obj['data']))
mkconst("text_start", obj['text_start'])
mkconst("data_start", obj['data_start'])
mkconst("bss_start", obj['bss_start'])
mkconst("entry", obj['entry'])
print("};")
