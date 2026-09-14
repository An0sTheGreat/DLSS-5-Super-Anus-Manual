"""Check diagnostic identities against a controlled fixture, not a game log."""
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
host = (root / "host.out").read_text()
log = (root / "ReShade.log").read_text(encoding="utf-8-sig")
known = re.search(r"Input trace fixture: SR=(\w+) FG=(\w+) motion=(\w+) depth=(\w+)", host)
assert known, "Missing fixture identities"
sr, fg, motion, depth = [int(value, 16) for value in known.groups()]
identity = re.search(r"Input trace SR identity: feature=(\w+) params=(\w+)", host)
assert identity, "Missing actual SR feature identity"
sr_feature, sr_parameters = [int(value, 16) for value in identity.groups()]
assert sr != fg, "Fixture must distinguish SR output from FG input"
assert "healthy FG callback NR=0 late FG callback NR=0 manual FG callback NR=0" in host
assert log.count("trace START:") == 1 and log.count("trace END:") == 1

def values(line):
    return dict(re.findall(r"([\w-]+)=(0x[\da-f]+|\d+)", line, re.I))

sources, inputs, submitted, pending = [], [], set(), {}
awaiting_post = {}
sr_pre, sr_post = 0, 0
events = 0
for line in log.splitlines():
    if "NR input trace " in line or "NR V6.6 trace gate:" in line or "NR V6.6 trace eval:" in line:
        events += 1
    if "NR input trace source-" in line:
        data = values(line)
        assert int(data["source"]) == 1 and int(data["frame"]) > 0
        assert (int(data["color"], 16), int(data["motion"], 16), int(data["depth"], 16)) == (sr, motion, depth)
        assert int(data["feature"], 16) == sr_feature and int(data["params"], 16) == sr_parameters
        key = (data["thread"], data["frame"], data["cmd"])
        if "source-begin:" in line:
            assert key not in pending
            assert data["thread"] not in awaiting_post, "Missing return inside the capture, not at its boundary"
            pending[key] = data
            awaiting_post[data["thread"]] = key
            sources.append(data)
        else:
            assert key in pending and int(data["result"], 16) & 255 == 1
            del pending[key]
    elif "NR input trace NGX-pre:" in line:
        data = values(line)
        feature = int(data["feature"], 16)
        assert feature in (0x12345678, sr_feature)
        if feature == sr_feature:
            assert data["thread"] not in awaiting_post, "New SR call before the preceding traced return"
            assert int(data["params"], 16) == sr_parameters
            assert int(data["caller-rva"], 16) != 0
            sr_pre += 1
        assert (int(data["backbuffer"], 16), int(data["motion"], 16), int(data["depth"], 16)) == (fg, motion, depth)
        assert int(data["hudless"], 16) == 0 and 1 <= int(data["MFG-index"]) <= 4
        assert int(data["native-cmd"], 16) != 0
        inputs.append(data)
    elif "NR input trace NGX-post:" in line:
        data = values(line)
        if int(data["feature"], 16) == sr_feature:
            assert int(data["params"], 16) == sr_parameters and int(data["result"], 16) == 1
            if int(data["nr-source"]):
                key = awaiting_post.pop(data["thread"])
                assert key not in pending, "Vendor return lacks its preceding source end"
                sr_post += 1
            else:
                assert data["thread"] not in awaiting_post, "Missing NR source return inside the capture"
    elif "NR input trace pre-submit:" in line:
        data = values(line)
        assert int(data["queue"], 16) != 0
        submitted.add(int(data["cmd"], 16))

assert 0 < events <= 4096
assert len(sources) >= 20 and len(inputs) >= 20
assert sr_pre >= 20 and sr_post == len(sources) - len(awaiting_post), "SR with stale DLSSG keys must correlate to native SR"
assert {int(item["cmd"], 16) for item in sources} & submitted
assert {int(item["native-cmd"], 16) for item in inputs} & submitted
# The deadline can expire inside the final callback even with no overflow.
# Permit only an unfinished tail per thread: any later SR pre/post above fails.
dropped = int(re.search(r"trace END: dropped=(\d+)", log).group(1))
assert set(pending) <= set(awaiting_post.values())
assert dropped == 0, "Controlled short capture must fit without loss"
if awaiting_post:
    print(f"Capture boundary: {len(awaiting_post)} unfinished trailing callback(s), not counted as complete returns.")
if "1.0.3-tlou2-boundary-trace.2" in log or "1.0.3-tlou2-nested-source.1" in log:
    for name in ("execute", "signal", "wait"):
        assert f"NR boundary hook {name}: enabled" in log, f"Native {name} hook did not activate"
    native_commands = {int(values(line)["cmd"], 16) for line in log.splitlines()
                       if "NR boundary native-pre-submit:" in line}
    assert {int(item["cmd"], 16) for item in sources} <= native_commands
    assert "NR boundary native-signal:" in log, "Missing native queue fence observation"
    mask = "0x78" if "1.0.3-tlou2-nested-source.1" in log else "0x38"
    assert f"active-mask={mask}" in log, "Fixture has queues but no Streamline: tag absence must not disable queues"
    if mask == "0x78":
        assert "NR boundary hook nested-source: enabled" in log
    print("PASS: native queue observers active without Streamline; all traced source commands reached native submission and fence observations exist.")
print(f"PASS: {len(sources)} source groups, {len(inputs)} FG inputs; distinct textures, command aliases, "
      f"submission correlation, {sr_pre} SR pre-observations with stale FG keys correctly correlated, "
      f"vendor parameters unchanged; {events} events, {dropped} dropped. No generated-frame validation.")
