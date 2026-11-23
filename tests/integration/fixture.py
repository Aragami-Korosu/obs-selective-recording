# SPDX-License-Identifier: GPL-2.0-or-later
"""Writes the starting scene collection, which needs a group (obs-websocket can't create groups)."""

import argparse
import json
import os
import uuid

GREEN, BLUE, RED = 0xFF00FF00, 0xFFFF0000, 0xFF0000FF


def color(name, value, w, h):
    return {"id": "color_source_v3", "versioned_id": "color_source_v3", "name": name, "uuid": str(uuid.uuid4()),
            "settings": {"color": value, "width": w, "height": h}}


def item(source, item_id, x, y, visible=True):
    return {"name": source["name"], "source_uuid": source["uuid"], "id": item_id, "visible": True if visible else False,
            "locked": False, "rot": 0.0, "pos": {"x": x, "y": y}, "scale": {"x": 1.0, "y": 1.0}, "align": 5,
            "bounds_type": 0, "bounds_align": 0, "bounds": {"x": 0.0, "y": 0.0}, "crop_left": 0, "crop_top": 0,
            "crop_right": 0, "crop_bottom": 0, "scale_filter": "disable", "blend_method": "default",
            "blend_type": "normal", "group_item_backup": False}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", required=True, help="OBS config folder (config/obs-studio)")
    args = parser.parse_args()

    bg = color("SR G BG", RED, 1920, 1080)
    box1 = color("SR G Box1", GREEN, 200, 200)
    box2 = color("SR G Box2", BLUE, 200, 200)

    group = {"id": "group", "versioned_id": "group", "name": "SR G Group", "uuid": str(uuid.uuid4()),
             "settings": {"items": [item(box1, 1, 0, 0), item(box2, 2, 300, 0)], "id_counter": 2,
                          "custom_size": True, "cx": 500, "cy": 200}}
    scene = {"id": "scene", "versioned_id": "scene", "name": "SR Group Scene", "uuid": str(uuid.uuid4()),
             "settings": {"items": [item(bg, 1, 0, 0), {**item(group, 2, 600, 300)}], "id_counter": 2,
                          "custom_size": False}}

    collection = {
        "name": "Test",
        "current_scene": scene["name"],
        "current_program_scene": scene["name"],
        "scene_order": [{"name": scene["name"]}],
        "sources": [bg, box1, box2, scene],
        "groups": [group],
        "transitions": [],
        "current_transition": "Fade",
        "transition_duration": 300,
    }

    scenes = os.path.join(args.config, "basic", "scenes")
    os.makedirs(scenes, exist_ok=True)
    with open(os.path.join(scenes, "Test.json"), "w", encoding="utf-8") as f:
        json.dump(collection, f, indent=2)


if __name__ == "__main__":
    main()
