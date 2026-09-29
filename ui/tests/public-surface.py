#!/usr/bin/env python3
"""Keep private MPC ABI details out of the public screen and API."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
public = (root / "include/mpclearn-ui.h").read_text()
example = (root / "examples/third-screen.cc").read_text()
example_header = (root / "examples/third-screen.h").read_text()
slider_example = (root / "examples/slider-screen.cc").read_text()
slider_example_header = (root / "examples/slider-screen.h").read_text()
toolkit_example = (root / "examples/toolkit-example.cc").read_text()
toolkit_example_header = (root / "examples/toolkit-example.h").read_text()
assignment = (root / "screens/global-midi-learn-screen.cc").read_text()
assignment_header = (root / "screens/global-midi-learn-screen.h").read_text()
groups = (root / "screens/global-midi-learn-groups.cc").read_text()
groups_header = (root / "screens/global-midi-learn-groups.h").read_text()
private = (root / "private/mpc-3.9.1-profile.h").read_text()

for name, text in (("public API", public), ("third screen", example),
                   ("third screen header", example_header),
                   ("slider screen", slider_example),
                   ("slider screen header", slider_example_header),
                   ("toolkit example", toolkit_example),
                   ("toolkit example header", toolkit_example_header),
                   ("assignment screen", assignment),
                   ("assignment screen header", assignment_header),
                   ("assignment groups", groups),
                   ("assignment groups header", groups_header)):
    assert not re.search(r"0x0[0-9a-fA-F]{6,}", text), f"raw host address in {name}"
    for word in ("vtable", "image_bias", "TextButton", "JUCE", "admission_cookie"):
        assert word not in text, f"private ABI word {word} in {name}"

assert '#include "third-screen.h"' in example and "mpclearn-ui.h" in example_header
for symbol in ("mpc_ui_app_register", "mpc_ui_app_open",
               "mpc_ui_app_request_close", "mpclearn_third_app_callbacks"):
    assert symbol in public + example + example_header, f"missing public app-host use: {symbol}"
assert '#include "slider-screen.h"' in slider_example and "mpclearn-ui.h" in slider_example_header
assert '#include "toolkit-example.h"' in toolkit_example and "mpclearn-ui.h" in toolkit_example_header
for symbol in ("mpc_ui_app_request_close", "mpc_ui_slider_create",
               "mpc_ui_label_create", "mpclearn_toolkit_example_app_callbacks"):
    assert symbol in toolkit_example + toolkit_example_header, f"missing combined public example use: {symbol}"
assert '#include "global-midi-learn-screen.h"' in assignment
assert "global-midi-learn-groups.h" in assignment_header
assert '#include "global-midi-learn-groups.h"' in groups
assert "mpclearn-ui.h" in groups_header
assert "private/" not in example and "private/" not in example_header
assert "private/" not in slider_example and "private/" not in slider_example_header
assert "private/" not in toolkit_example and "private/" not in toolkit_example_header
assert "private/" not in assignment and "private/" not in assignment_header
assert "private/" not in groups and "private/" not in groups_header
offsets = re.findall(r"#define MPC_UI_RVA_[A-Z_]+ (0x[0-9a-f]+)u", private)
assert len(offsets) == 42 and len(set(offsets)) == len(offsets)
assert "bc054a3f3ba02c2d33ac9a515a4a8638964da779223502286d6a64b517bf1426" in private
print("PASS public API/screens contain no raw host addresses or ABI vocabulary; pinned profile remains private")
