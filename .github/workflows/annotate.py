"""Re-print the gate's report as GitHub annotations.

A job's log needs authentication to read; a check run's annotations do not, so
this is how the result reaches anyone reading the public API: an error per
failing check and its first detail lines, and a notice per build- or gpu-tier
check (configure, build, warnings, ctest, parity) plus the closing count, so a
green job shows what it ran -- a SKIP is green too. It reads `check.py`'s own
report and decides nothing: the gate's exit code has already set the job's.
"""
import re
import sys

MAX = 10    # GitHub keeps at most ten annotations of each level per step
NOTICED = ("configure", "build", "warnings", "ctest", "parity")


def main(path, failed_run):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            lines = f.read().splitlines()
    except OSError:
        print("::error title=gate::no gate log at %s" % path)
        return 0
    out, notes, failed, section = [], [], set(), None
    for line in lines:
        m = re.match(r"^\[(\w+)\s*\]\s+(\S+)\s+(.*?)\s+([\d.]+)s$", line)
        if m:
            status, cid, summary, secs = m.groups()
            if status == "FAIL":
                failed.add(cid)
                out.append("%s: %s" % (cid, summary))
            elif cid in NOTICED:
                notes.append("%s %s: %s (%ss)" % (status, cid, summary, secs))
            continue
        if re.match(r"^\d+ checks: ", line):
            notes.append(line.strip())
            continue
        m = re.match(r"^--- (\S+) ---$", line)
        if m:
            section = m.group(1)
            continue
        if section in failed and line.startswith("  ") and line.strip():
            out.append("%s: %s" % (section, line.strip()))
    if not out and failed_run:
        out = [l for l in lines if l.strip()][-MAX:] or ["gate failed, empty log"]
    for level, msgs in (("error", out), ("notice", notes)):
        for msg in msgs[:MAX]:
            print("::%s title=gate::%s" % (level, msg.replace("%", "%25")))
    return 0


if __name__ == "__main__":
    # usage: annotate.py <gate.log> <job status>
    sys.exit(main(sys.argv[1], sys.argv[2] != "success"))
