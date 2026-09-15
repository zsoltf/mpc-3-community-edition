#!/usr/bin/env python3
"""Derive MCU pages from the pinned firmware's native skin catalogue.

Usage: effects-catalogue.py EXTRACTED/Content/Synths OUTPUT.h
Only a qualified Bank Direction is transformed. Unsupported whole entries are
reported, never partly guessed. The runtime adds native indices absent here.
"""
import argparse
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET

COLUMN = (13, 9, 5, 1, 14, 10, 6, 2, 15, 11, 7, 3, 16, 12, 8, 4)
# Granulator native TUI row coordinates qualify this authored direction.
ROW_TOP_DOWN = (13, 14, 15, 16, 9, 10, 11, 12, 5, 6, 7, 8, 1, 2, 3, 4)
DIRECTIONS = {"Column": COLUMN, "RowTopDown": ROW_TOP_DOWN}


def derive(root):
    entries, omitted, seen_ids, owners = [], [], set(), {}
    digest = hashlib.sha256()
    files = sorted(root.glob("*/Plugin Skins/Q-Links.json"))
    for path in files:
        folder = path.parent.parent
        sources = (folder / "version.xml", path, path.parent / "TUI.json")
        for source in sources:
            data = source.read_bytes()
            digest.update(source.relative_to(root).as_posix().encode() + b"\0")
            digest.update(len(data).to_bytes(8, "little") + data)
        version = ET.fromstring(sources[0].read_bytes())
        identifier = version.findtext("identifier")
        if not identifier or identifier in seen_ids:
            raise ValueError(f"Missing or duplicate content identifier: {folder.name}")
        seen_ids.add(identifier)
        links = json.loads(path.read_bytes())
        screen = links["Screen Mode Q-Links"]
        if links["version"] != 4 or screen["version"] != 4:
            raise ValueError(f"Unqualified schema: {folder.name}")
        tabs = json.loads(sources[2].read_bytes())["pageData"]["tabs"]
        labels = {}
        for tab in tabs:
            key = (tab["fnKeyIndex"] + 1, tab["fnKeySubIndex"] + 1)
            if key in labels:
                raise ValueError(f"Duplicate TUI tab: {folder.name} {key}")
            labels[key] = tab["tabName"]
        directions = sorted({m["Bank Direction"] for m in screen["map"]})
        if any(direction not in DIRECTIONS for direction in directions):
            omitted.append((identifier, ",".join(directions)))
            continue
        pages, map_keys = [], set()
        for mapping in screen["map"]:
            key = (mapping["Tab"], mapping["SubTab"])
            if key in map_keys or key not in labels:
                raise ValueError(f"Ambiguous native tab mapping: {folder.name} {key}")
            map_keys.add(key)
            qlinks = mapping["Q-Links"]
            if set(qlinks) != {f"Q-Link {i}" for i in range(1, 17)}:
                raise ValueError(f"Incomplete native Q-Link map: {folder.name} {key}")
            values = [qlinks[f"Q-Link {i}"] for i in DIRECTIONS[mapping["Bank Direction"]]]
            if any(type(v) is not int or not -1 <= v < 4096 for v in values):
                raise ValueError(f"Invalid native parameter index: {folder.name} {key}")
            # Repeated indices are authored controls on multiple native tabs.
            # Preserve them and all holes; drop only a completely empty page.
            for half in range(2):
                indices = values[half * 8:half * 8 + 8]
                if any(v >= 0 for v in indices):
                    pages.append((key[0], key[1], half, labels[key], indices))
        if not pages:
            omitted.append((identifier, "empty-screen-map"))
            continue
        if len(pages) > 64:
            raise ValueError(f"Catalogue page bound exceeded: {folder.name}")
        identities = {}
        for preset in sorted(folder.glob("Presets/**/*.xpl")):
            plugin = ET.parse(preset).find(".//PLUGIN")
            if plugin is None:
                continue
            a = plugin.attrib
            identity = (a["name"], a["format"], int(a["uid"], 16), int(a["isInstrument"]))
            if not identity[0] or not identity[1] or not 0 <= identity[2] <= 0xffffffff or identity[3] not in (0, 1):
                raise ValueError(f"Invalid native PluginDescription tuple: {preset}")
            # The first sorted preset for each exact tuple is reproducible provenance.
            identities.setdefault(identity, preset)
        if not identities:
            omitted.append((identifier, "no-native-description-tuple"))
            continue
        for identity, preset in identities.items():
            if identity in owners and owners[identity] != identifier:
                raise ValueError(f"Ambiguous native PluginDescription tuple: {identity}")
            owners[identity] = identifier
            data = preset.read_bytes()
            digest.update(preset.relative_to(root).as_posix().encode() + b"\0")
            digest.update(len(data).to_bytes(8, "little") + data)
        entries.append((identifier, version.findtext("version") or "", pages, identities))
    return entries, omitted, len(files), digest.hexdigest()


def render(entries, omitted, count, digest):
    quote = lambda value: json.dumps(value, ensure_ascii=True)
    lines = ["/* Generated by effects-catalogue.py; do not edit.",
             f" * Source files: {count} Q-Links v4 plus matching TUI/version.xml and tuple source XPLs.",
             f" * Sorted relative paths/lengths/bytes SHA256: {digest}",
             " * Native direction/pairs preserve repetitions; -1 denotes padding.",
             " * Omitted entries retain native enumeration at runtime:"]
    lines.extend(f" * {identifier}: {reason}" for identifier, reason in omitted)
    lines += [" */", "#ifndef MPC_EFFECTS_CATALOGUE_H", "#define MPC_EFFECTS_CATALOGUE_H",
              "#include <stdint.h>",
              "typedef struct {uint16_t tab,subtab,half;const char *name;int16_t indices[8];} EffectCataloguePage;",
              "typedef struct {const char *identifier,*content_version;unsigned first,count;} EffectCatalogue;",
              "typedef struct {const char *name,*format;uint32_t uid,instrument,catalogue;const char *source;} EffectCatalogueIdentity;",
              "static const EffectCataloguePage effect_catalogue_pages[]={"]
    for _, _, pages, _ in entries:
        for tab, subtab, half, label, indices in pages:
            lines.append(f" {{{tab},{subtab},{half},{quote(label)},{{{','.join(map(str, indices))}}}}},")
    lines += ["};", "static const EffectCatalogue effect_catalogue[]={"]
    offset = 0
    for identifier, version, pages, _ in entries:
        lines.append(f" {{{quote(identifier)},{quote(version)},{offset},{len(pages)}}},")
        offset += len(pages)
    lines += ["};", "static const EffectCatalogueIdentity effect_catalogue_identities[]={"]
    for entry, (_, _, _, identities) in enumerate(entries):
        for (name, format_name, uid, instrument), source in identities.items():
            source = "/".join(source.parts[-3:])
            lines.append(f" {{{quote(name)},{quote(format_name)},0x{uid:08x}u,{instrument},{entry},{quote(source)}}},")
    lines += ["};", "#endif", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    entries, omitted, count, digest = derive(args.root)
    args.output.write_text(render(entries, omitted, count, digest), encoding="ascii")
    print(f"{count} source plugins; {len(entries)} catalogued; {len(omitted)} omitted; "
          f"{sum(len(e[2]) for e in entries)} pages; source digest {digest}")
    for identifier, reason in omitted:
        print(f"fallback {identifier}: {reason}")


if __name__ == "__main__":
    main()
