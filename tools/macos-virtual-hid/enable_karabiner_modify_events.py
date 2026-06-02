#!/usr/bin/env python3

import argparse
import copy
import json
import os
import pathlib
import shutil
import sys
import tempfile
import time


DEFAULT_VENDOR_ID = 0x1209
DEFAULT_PRODUCT_ID = 0x4B42

KARABINER_OUTPUT_VENDOR_ID = 1452
KARABINER_OUTPUT_PRODUCT_ID = 591


def parse_int(value):
    return int(value, 0)


def find_profile(config, profile_name):
    profiles = config.get("profiles")
    if not isinstance(profiles, list):
        raise ValueError("karabiner.json does not contain a profiles array")

    if profile_name:
        for profile in profiles:
            if profile.get("name") == profile_name:
                return profile
        raise ValueError(f"profile not found: {profile_name}")

    for profile in profiles:
        if profile.get("selected") is True:
            return profile

    if len(profiles) == 1:
        return profiles[0]

    raise ValueError("no selected profile found; pass --profile")


def identifiers_match(identifiers, vendor_id, product_id):
    return (
        identifiers.get("vendor_id") == vendor_id
        and identifiers.get("product_id") == product_id
        and identifiers.get("is_keyboard") is True
    )


def ensure_barrier_device(profile, vendor_id, product_id):
    devices = profile.setdefault("devices", [])
    if not isinstance(devices, list):
        raise ValueError("selected profile devices is not an array")

    for device in devices:
        identifiers = device.get("identifiers")
        if isinstance(identifiers, dict) and identifiers_match(
            identifiers, vendor_id, product_id
        ):
            device["ignore"] = False
            device.setdefault("simple_modifications", [])
            identifiers["is_keyboard"] = True
            identifiers["is_pointing_device"] = False
            return "updated", device

    device = {
        "identifiers": {
            "is_keyboard": True,
            "is_pointing_device": False,
            "vendor_id": vendor_id,
            "product_id": product_id,
        },
        "ignore": False,
        "simple_modifications": [],
    }
    devices.append(device)
    return "added", device


def find_karabiner_output_device_entries(profile):
    result = []
    for device in profile.get("devices", []):
        identifiers = device.get("identifiers")
        if not isinstance(identifiers, dict):
            continue
        if (
            identifiers.get("vendor_id") == KARABINER_OUTPUT_VENDOR_ID
            and identifiers.get("product_id") == KARABINER_OUTPUT_PRODUCT_ID
            and identifiers.get("is_keyboard") is True
        ):
            result.append(device)
    return result


def write_json_atomic(path, data):
    fd, tmp_name = tempfile.mkstemp(
        prefix=f".{path.name}.", suffix=".tmp", dir=str(path.parent)
    )
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as tmp:
            json.dump(data, tmp, ensure_ascii=False, indent=4)
            tmp.write("\n")
        os.replace(tmp_name, path)
    except Exception:
        try:
            os.unlink(tmp_name)
        except OSError:
            pass
        raise


def print_rule_snippets(vendor_id, product_id):
    device_if = {
        "type": "device_if",
        "identifiers": [
            {
                "vendor_id": vendor_id,
                "product_id": product_id,
                "is_keyboard": True,
            }
        ],
    }
    device_unless = {
        "type": "device_unless",
        "identifiers": [
            {
                "vendor_id": KARABINER_OUTPUT_VENDOR_ID,
                "product_id": KARABINER_OUTPUT_PRODUCT_ID,
                "is_keyboard": True,
            }
        ],
    }

    print()
    print("Use this condition on Barrier-input-only rules:")
    print(json.dumps(device_if, ensure_ascii=False, indent=2))
    print()
    print("Use this condition if you also want to explicitly exclude Karabiner output:")
    print(json.dumps(device_unless, ensure_ascii=False, indent=2))


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Enable Karabiner Modify events for the Barrier input virtual keyboard."
        )
    )
    parser.add_argument(
        "--config",
        default=os.path.expanduser("~/.config/karabiner/karabiner.json"),
        help="Path to karabiner.json",
    )
    parser.add_argument(
        "--profile",
        help="Profile name. Defaults to the selected profile.",
    )
    parser.add_argument(
        "--vendor-id",
        type=parse_int,
        default=DEFAULT_VENDOR_ID,
        help="Barrier input keyboard vendor id. Default: 0x1209",
    )
    parser.add_argument(
        "--product-id",
        type=parse_int,
        default=DEFAULT_PRODUCT_ID,
        help="Barrier input keyboard product id. Default: 0x4b42",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the modified JSON without writing it.",
    )
    parser.add_argument(
        "--no-backup",
        action="store_true",
        help="Do not create a timestamped backup before writing.",
    )
    args = parser.parse_args()

    path = pathlib.Path(args.config).expanduser()
    with path.open("r", encoding="utf-8") as f:
        original = json.load(f)

    updated = copy.deepcopy(original)
    profile = find_profile(updated, args.profile)
    action, device = ensure_barrier_device(
        profile, args.vendor_id, args.product_id
    )
    changed = updated != original

    output_entries = find_karabiner_output_device_entries(profile)

    if args.dry_run:
        json.dump(updated, sys.stdout, ensure_ascii=False, indent=4)
        sys.stdout.write("\n")
    elif not changed:
        print("unchanged: Barrier input virtual keyboard device entry")
        print(f"config: {path}")
    else:
        if not args.no_backup:
            backup = path.with_suffix(
                path.suffix + time.strftime(".backup-%Y%m%d-%H%M%S")
            )
            shutil.copy2(path, backup)
            print(f"backup: {backup}")

        write_json_atomic(path, updated)
        print(f"{action}: Barrier input virtual keyboard device entry")
        print(f"config: {path}")

    print("device entry:")
    print(json.dumps(device, ensure_ascii=False, indent=2))

    if output_entries:
        print()
        print(
            "Karabiner output virtual keyboard already has a profile device entry; "
            "left it unchanged."
        )
    else:
        print()
        print(
            "Karabiner output virtual keyboard was not added to devices; "
            "Barrier input and Karabiner output remain distinct by vendor/product id."
        )

    print_rule_snippets(args.vendor_id, args.product_id)


if __name__ == "__main__":
    main()
