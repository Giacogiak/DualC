#!/usr/bin/env python3
"""The plan-run `remote-run` check: the pushed branch's latest GitHub Actions run is green.

Reads the current branch and HEAD, asks the public Actions API (no token: the repo is
public) for the newest run of that branch, waits while it is queued or in progress, and
exits 0 only when a run for exactly this HEAD completed with conclusion `success`.
Polls every REMOTE_RUN_POLL_SECONDS (default 180: the unauthenticated API allows 60
requests an hour, shared with the session's own reads). Jobs
marked `continue-on-error` do not affect a run's conclusion, so an allowed-to-fail
experiment never turns this check red; a required job failing does.

Exit codes: 0 green; 1 red, no run for HEAD, or timed out; 2 could not reach the API.
Lines starting with FAIL are what the driver quotes.
"""
import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.request

REPO = os.environ.get("REMOTE_RUN_REPO", "Giacogiak/DualC")
WAIT_MINUTES = int(os.environ.get("REMOTE_RUN_WAIT_MINUTES", "45"))
POLL_SECONDS = int(os.environ.get("REMOTE_RUN_POLL_SECONDS", "180"))


def git(*args):
    return subprocess.check_output(["git", *args], text=True).strip()


def api(url):
    req = urllib.request.Request(url, headers={"User-Agent": "dualc-plan-run",
                                               "Accept": "application/vnd.github+json"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.load(r)


def main():
    branch = git("rev-parse", "--abbrev-ref", "HEAD")
    head = git("rev-parse", "HEAD")
    if branch == "HEAD":
        print("FAIL detached HEAD; the check needs a branch that was pushed")
        return 1
    url = ("https://api.github.com/repos/%s/actions/runs?branch=%s&per_page=5"
           % (REPO, branch))
    deadline = time.time() + WAIT_MINUTES * 60
    while True:
        try:
            runs = api(url).get("workflow_runs", [])
        except (urllib.error.URLError, OSError) as e:
            print("FAIL cannot reach the GitHub API: %s" % e)
            return 2
        mine = [r for r in runs if r.get("head_sha") == head]
        if not mine:
            print("FAIL no run for HEAD %s on branch %s (was it pushed?)" % (head[:7], branch))
            return 1
        run = mine[0]
        print("run %s  status=%s  conclusion=%s" % (run["html_url"], run["status"],
                                                   run["conclusion"]))
        if run["status"] == "completed":
            if run["conclusion"] == "success":
                print("OK branch %s HEAD %s green" % (branch, head[:7]))
                return 0
            try:
                jobs = api(run["jobs_url"]).get("jobs", [])
                for j in jobs:
                    if j.get("conclusion") not in ("success", "skipped", None):
                        print("FAIL job %s: %s" % (j["name"], j["conclusion"]))
            except (urllib.error.URLError, OSError):
                pass
            print("FAIL run conclusion is %s: %s" % (run["conclusion"], run["html_url"]))
            return 1
        if time.time() > deadline:
            print("FAIL run still %s after %d minutes: %s"
                  % (run["status"], WAIT_MINUTES, run["html_url"]))
            return 1
        time.sleep(POLL_SECONDS)


if __name__ == "__main__":
    sys.exit(main())
