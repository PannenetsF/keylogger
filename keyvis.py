import argparse
import json
import os
import xml.etree.ElementTree as ET

keycode2key = {
    # 数字键（主键盘区）
    29: "0",
    18: "1",
    19: "2",
    20: "3",
    21: "4",
    23: "5",
    22: "6",
    26: "7",
    28: "8",
    25: "9",
    # 字母键
    0: "A",
    11: "B",
    8: "C",
    2: "D",
    14: "E",
    3: "F",
    5: "G",
    4: "H",
    34: "I",
    38: "J",
    40: "K",
    37: "L",
    46: "M",
    45: "N",
    31: "O",
    35: "P",
    12: "Q",
    15: "R",
    1: "S",
    17: "T",
    32: "U",
    9: "V",
    13: "W",
    7: "X",
    16: "Y",
    6: "Z",
    # 符号键（主键盘区）
    50: "`",
    27: "-",
    24: "=",
    33: "[",
    30: "]",
    42: "\\",
    41: ";",
    39: "'",
    43: ",",
    47: ".",
    44: "/",
    # 功能键
    53: "ESC",  # Esc 键 ← 已补充
    36: "Enter",  # 回车键
    48: "Tab",  # Tab 键
    51: "Back",  # Delete 键
    117: "Del",
    49: "",  # 空格键
    # 修饰键
    54: "Right Meta",  # 或 "RIGHT_META"
    55: "Left Meta",  # 或 "META"
    56: "Shift",
    57: "Caps",
    58: "Left Opt",  # 或 "OPTION"
    59: "^",
    60: "Right Shift",
    61: "Right Opt",  # 或 "RIGHT_OPTION"
    # 方向
    123: "Left",  # ← 方向键
    124: "Right",  # → 方向键
    125: "Down",  # ↓ 方向键
    126: "Up",  # ↑ 方向键
}

key2keycode = {v: k for k, v in keycode2key.items()}


def parse():
    parser = argparse.ArgumentParser(
        description="Visualize keypoints from a JSON file."
    )
    parser.add_argument(
        "file_prefix", type=str, help="Path to the JSON file containing keypoints"
    )
    parser.add_argument(
        "keycounts", type=str, help="Path to the JSON file containing key counts"
    )
    return parser.parse_args()


def color_to_hex(color):
    if isinstance(color, str):
        return color
    elif isinstance(color, tuple) and len(color) == 3:
        return "#{:02x}{:02x}{:02x}".format(*color)
    else:
        raise ValueError("Invalid color format")


def count_norm(keycounts: dict, key2keycode: dict):
    # 0 light blue
    # max dark red
    # linear scale
    white = (255, 255, 255)
    light_blue = (173, 216, 230)  # 00bfff
    dark_red = (139, 0, 0)  # 8b0000
    max_counts = max([v for k, v in keycounts.items() if isinstance(v, int)])
    cnt2color = {}
    # interpolate colors from light blue to dark red, if the count is 0, white
    for k, v in keycounts.items():
        if v == 0:
            color = white
        ratio = v / max_counts
        color = (
            int(light_blue[0] + (dark_red[0] - light_blue[0]) * ratio),
            int(light_blue[1] + (dark_red[1] - light_blue[1]) * ratio),
            int(light_blue[2] + (dark_red[2] - light_blue[2]) * ratio),
        )
        cnt2color[k] = color

    for k, v in key2keycode.items():
        if v not in cnt2color:
            cnt2color[v] = white
    return cnt2color


def read_log(fn):
    # like keycode 2 count
    # 8: 1
    # 36: 7
    data = {}
    for line in open(fn, "r"):
        line = line.strip()
        if len(line) == 0:
            continue
        try:
            k, v = line.split(":")
            k = int(k.strip())
            v = int(v.strip())
            data[k] = v
        except ValueError:
            print(f"Invalid line: {line}")
    return data


# keycode to key


def main():
    args = parse()
    json_config_path = args.file_prefix + ".json"
    svg_template_path = args.file_prefix + ".svg"
    json_config = json.load(open(json_config_path, "r"))
    key_order = sum(json_config, [])
    key_order = [k.split("\n")[-1] for k in key_order if isinstance(k, str)]
    # change the same key to left key and right key. eg there are 2 key meta
    duplicated = {}
    for idx, k in enumerate(key_order):
        if k in duplicated:
            key_order[idx] = "Right " + k
            key_order[duplicated[k]] = "Left " + k
        else:
            duplicated[k] = idx

    if os.path.isdir(args.keycounts):
        # get all files in format of yyyy-mm-dd$ with re. there is no json file.
        import re

        files = os.listdir(args.keycounts)
        pattern = re.compile(r"^\d{4}-\d{2}-\d{2}.log$")
        files = [f for f in files if pattern.match(f)]
        data_list = [read_log(os.path.join(args.keycounts, f)) for f in files]
    else:
        # read the file directly
        data_list = [read_log(args.keycounts)]

    # merge those dict
    keycounts = {}
    for data in data_list:
        for k, v in data.items():
            if k not in keycounts:
                keycounts[k] = 0
            keycounts[k] += v
    for k in keycode2key:
        if k not in keycounts:
            keycounts[k] = 0

    count2color = count_norm(keycounts, key2keycode)

    print(key_order)
    xml_tree = ET.parse(svg_template_path)
    xml_root = xml_tree.getroot()
    cnt = 0
    for xc in xml_root.iter():
        if xc.attrib.get("class", "") == "  keycap":
            cnt += 1
            key = key_order.pop(0)
            key_code = key2keycode.get(key, None)
            if not key_code:
                continue
            for xcc in xc.iter():
                pass
            color = count2color[keycounts[key_code]]
            xcc.attrib["fill"] = color_to_hex(color)
            print(f'{key} {key_code} {keycounts[key_code]} {color}')
            # color_to_hex((255, 0, 0))
    # save svg to new file
    xml_tree.write(args.file_prefix + "_out.svg")

    assert 0 == len(key_order), f"Mismatch in number of keys: {cnt} vs {len(key_order)}"


if __name__ == "__main__":
    main()
