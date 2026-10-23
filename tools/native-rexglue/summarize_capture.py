#!/usr/bin/env python3
"""Summarize RenderDoc Vulkan XML commands; counts do not prove correct pixels."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("xml", type=Path)
args = parser.parse_args()
commands, formats, colors = Counter(), Counter(), Counter()
indices = vertices = 0

def number(chunk, name, default=0):
    node = chunk.find(f".//*[@name='{name}']")
    return int(node.text) if node is not None and node.text else default

for _, chunk in ET.iterparse(args.xml, events=("end",)):
    if chunk.tag != "chunk":
        continue
    name = chunk.get("name", "")
    commands[name] += 1
    if name == "vkCmdDrawIndexed":
        indices += number(chunk, "indexCount") * number(chunk, "instanceCount", 1)
    elif name == "vkCmdDraw":
        vertices += number(chunk, "vertexCount") * number(chunk, "instanceCount", 1)
    elif name in ("vkCreateRenderPass", "vkCreateRenderPass2"):
        for node in chunk.findall(".//*[@name='colorAttachmentCount']"):
            colors[node.text] += 1
    elif name == "vkCreateImage":
        for node in chunk.findall(".//*[@name='format']"):
            formats[node.get("string", node.text)] += 1
    chunk.clear()
with args.xml.open("rb") as stream:
    digest = hashlib.file_digest(stream, "sha256").hexdigest()
print(json.dumps({"source": str(args.xml), "sha256": digest,
    "scope": "recorded Vulkan commands; scene identity, pixels and parity unverified",
    "commands": dict(sorted(commands.items())), "indexed_elements": indices,
    "nonindexed_vertices": vertices, "created_image_formats": dict(formats),
    "created_subpass_color_attachment_counts": dict(colors)}, indent=2))
