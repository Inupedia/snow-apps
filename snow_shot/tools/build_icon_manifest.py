#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


IMAGEGEN_ICON_ALIASES = {
    "keyboard": "custom-application-shortcuts-nav.svg",
    "mouse": "custom-global-mouse-nav.svg",
    "ocr-translate": "custom-translation.svg",
    "pin-to-screen": "custom-pin-screen-nav.svg",
    "pin-to-screen-management": "custom-pin-management-nav.svg",
    "quick-save": "custom-download.svg",
    "record-screen": "custom-screen-recording-nav.svg",
    "save": "custom-file-add.svg",
    "screenshot-feature": "custom-picture.svg",
    "screenshot-copy": "custom-copy.svg",
    "screenshot-delay": "custom-clock-circle.svg",
    "screenshot-focused-window": "custom-desktop.svg",
    "screenshot-full-screen": "custom-fullscreen.svg",
    "text-recognition": "custom-text-recognition-translation-nav.svg",
    "wheel-mouse": "custom-global-mouse-nav.svg",
}


def symbol(name: str) -> str:
    if name.startswith("custom-"):
        name = name[len("custom-") :]
    return "".join(part[:1].upper() + part[1:] for part in re.findall(r"[A-Za-z0-9]+", name))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    asset_root = root / "src" / "presentation" / "components" / "icons" / "resources"
    entries = []
    for asset in sorted(asset_root.glob("*.svg"), key=lambda item: item.name):
        name = asset.stem
        imagegen_source = asset_root / IMAGEGEN_ICON_ALIASES.get(name, asset.name)
        variant = "outlined"
        model = "monochrome"
        defaults = None
        allow_embedded_data_images = (
            (name.startswith("custom-") or name in IMAGEGEN_ICON_ALIASES)
            and "data:image/" in imagegen_source.read_text(encoding="utf-8")
        )
        if name == "screenshot-feature":
            variant = "twotone"
            if not allow_embedded_data_images:
                model = "twoTone"
                defaults = {"secondary": "#9254DE"}
        elif allow_embedded_data_images:
            variant = "outlined"
            if name == "custom-setting":
                model = "fullColor"
        elif name in {"snow-shot-logo", "snow-shot-mini-logo"}:
            variant = "brand"
        elif name == "selection-shadow-cursor":
            variant, model = "cursor", "fullColor"
        entry = {
            "variant": variant,
            "symbol": symbol(name),
            "name": name,
            "source": f"../src/presentation/components/icons/resources/{imagegen_source.name}",
            "colorModel": model,
            "fit": "contain",
        }
        if defaults:
            entry["defaultColors"] = defaults
        if allow_embedded_data_images:
            entry["allowEmbeddedDataImages"] = True
        entries.append(entry)
    entries.append({
        "variant": "app",
        "symbol": "ApplicationIcon",
        "name": "application-icon",
        "source": "app-icon.svg",
        "colorModel": "fullColor",
        "fit": "contain",
        "allowEmbeddedDataImages": True,
    })
    entries.append({
        "variant": "app",
        "symbol": "ApplicationTitleBarIcon",
        "name": "application-title-bar-icon",
        "source": "app-icon-titlebar.svg",
        "colorModel": "fullColor",
        "fit": "contain",
        "allowEmbeddedDataImages": True,
    })
    entries.sort(key=lambda item: (item["variant"], item["name"]))
    manifest = {
        "schemaVersion": 1,
        "pack": "snow-shot",
        "cppNamespace": "snow_shot::presentation::icons::custom",
        "headerInclude": "snow_shot/presentation/components/icons/snowshoticons.h",
        "source": "InuShot project-owned static SVG assets",
        "entries": entries,
    }
    expected = json.dumps(manifest, indent=2) + "\n"
    output = root / "resources" / "icons.manifest.json"
    if args.check:
        actual = output.read_text(encoding="utf-8") if output.exists() else ""
        if actual != expected:
            raise SystemExit(f"stale manifest: {output}")
    else:
        output.write_text(expected, encoding="utf-8", newline="\n")
    print(f"snow-shot manifest entries={len(entries)} check={args.check}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
