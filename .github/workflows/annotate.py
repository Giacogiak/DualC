"""Re-print the gate's failures as GitHub error annotations.

A job's log needs authentication to read; a check run's annotations do not, so
this is how a failing check's name and first detail lines reach anyone reading
the public API. It reads `check.py`'s own report and decides nothing: the gate's
exit code has already failed the job.
"""
import re
import sys

MAX = 10    # GitHub shows at most ten error annotations per step


def main(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            lines = f.read().splitlines()
    except OSError:
        print("::error title=gate::no gate log at %s" % path)
        return 0
    out, failed, section = [], set(), None
    for line in lines:
        m = re.match(r"^\[FAIL\]\s+(\S+)\s+(.*?)\s+[\d.]+s$", line)
        if m:
            failed.add(m.group(1))
            out.append("%s: %s" % (m.group(1), m.group(2)))
            continue
        m = re.match(r"^--- (\S+) ---$", line)
        if m:
            section = m.group(1)
            continue
        if section in failed and line.startswith("  ") and line.strip():
            out.append("%s: %s" % (section, line.strip()))
    if not out:
        out = [l for l in lines if l.strip()][-MAX:] or ["gate failed, empty log"]
    for msg in out[:MAX]:
        print("::error title=gate::%s" % msg.replace("%", "%25").replace("\n", "%0A"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "gate.log"))
