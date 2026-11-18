#!/usr/bin/env python3
"""经 GitHub Git Data API 把本地若干提交原样推到远端（沙箱代理只放行 api.github.com）。

用法：push_via_api.py <repo_dir> <branch> <expected_remote_sha> <sha1> [sha2 ...]
安全阀：每个提交的 tree 必须与本地 `git rev-parse <sha>^{tree}` 完全一致，
否则立即中止、不更新 ref。
"""
import base64
import json
import os
import subprocess
import sys
import urllib.request

API = "https://api.github.com"
OWNER, REPO = "twipty", "ros-chinese-ime"


def token():
    with open(os.path.expanduser("~/mygithubpak/key.txt")) as f:
        return f.read().strip()


TOK = token()


def call(method, path, payload=None):
    data = json.dumps(payload).encode() if payload is not None else None
    req = urllib.request.Request(API + path, data=data, method=method)
    req.add_header("Authorization", "Bearer " + TOK)
    req.add_header("Accept", "application/vnd.github+json")
    req.add_header("X-GitHub-Api-Version", "2022-11-28")
    if data:
        req.add_header("Content-Type", "application/json")
    with urllib.request.urlopen(req, timeout=60) as r:
        body = r.read()
    return json.loads(body) if body else {}


def git(repo, *args, binary=False):
    out = subprocess.run(["git", "-C", repo] + list(args),
                         check=True, capture_output=True).stdout
    return out if binary else out.decode().strip()


def main():
    repo, branch, expected, shas = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]

    ref = call("GET", f"/repos/{OWNER}/{REPO}/git/ref/heads/{branch}")
    head = ref["object"]["sha"]
    if head != expected:
        sys.exit(f"远端 {branch} 已是 {head}，与预期 {expected} 不符，中止")
    parent_sha, parent_tree = head, call(
        "GET", f"/repos/{OWNER}/{REPO}/git/commits/{head}")["tree"]["sha"]
    print(f"远端起点 {head[:7]} tree={parent_tree[:7]}")

    for sha in shas:
        local_tree = git(repo, "rev-parse", f"{sha}^{{tree}}")
        raw = git(repo, "diff", "--name-status", f"{sha}^", sha, binary=True).decode()
        rows = [l.split("\t") for l in raw.splitlines() if l.strip()]
        entries = []
        for row in rows:
            status, path = row[0][0], row[-1]
            if status == "D":
                entries.append({"path": path, "mode": "100644", "type": "blob", "sha": None})
                continue
            content = git(repo, "show", f"{sha}:{path}", binary=True)
            blob = call("POST", f"/repos/{OWNER}/{REPO}/git/blobs", {
                "content": base64.b64encode(content).decode(), "encoding": "base64"})
            mode = "100755" if git(repo, "ls-tree", sha, "--", path).startswith("100755") else "100644"
            entries.append({"path": path, "mode": mode, "type": "blob", "sha": blob["sha"]})
            print(f"  blob {path} {blob['sha'][:7]} {len(content)}B")

        tree = call("POST", f"/repos/{OWNER}/{REPO}/git/trees",
                    {"base_tree": parent_tree, "tree": entries})
        if tree["sha"] != local_tree:
            sys.exit(f"tree 不一致：远端 {tree['sha']} vs 本地 {local_tree}（已中止，ref 未动）")

        meta = git(repo, "log", "-1", "--format=%an|%ae|%aI|%cn|%ce|%cI|%B", sha)
        an, ae, ai, cn, ce, ci, msg = meta.split("|", 6)
        c = call("POST", f"/repos/{OWNER}/{REPO}/git/commits", {
            "message": msg, "tree": tree["sha"], "parents": [parent_sha],
            "author": {"name": an, "email": ae, "date": ai},
            "committer": {"name": cn, "email": ce, "date": ci}})
        print(f"commit {sha[:7]} -> {c['sha'][:7]}  tree {tree['sha'][:7]} ✅")
        parent_sha, parent_tree = c["sha"], tree["sha"]

    call("PATCH", f"/repos/{OWNER}/{REPO}/git/refs/heads/{branch}",
         {"sha": parent_sha, "force": False})
    print(f"{branch} 已前移到 {parent_sha}")


if __name__ == "__main__":
    main()
