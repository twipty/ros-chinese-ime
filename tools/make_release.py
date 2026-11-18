#!/usr/bin/env python3
"""建 annotated tag + GitHub release 并上传资产（只走 api.github.com / uploads.github.com）。

**这不是首选路径**（2026-11-18 起）。正常发版 = 写好 `docs/releases/<tag>.md`、bump
`ime/version.h`、push tag，由 CI 的 `release` job 用本次运行的产物发布并回验资产
（见 HANDOVER §11.20）。本脚本保留为兜底：CI 不可用、或本机需要绕过 git push 建 tag 时用。

用途：本机 git push 走不通时（沙箱代理只放行 api.github.com），发版仍能完成。
前置：① 目标 commit 已经在远端 main 上（用 tools/push_via_api.py 推）；
     ② `python3 tools/check_release_version.py <tag>` 已全绿（否则发出去的版本号是假的）。

用法：
    make_release.py <tag> <target_sha> <assets_dir> <notes_file> [--title T] [--final]
    make_release.py alpha0.0.3 0e4fe1e... /path/to/assets /path/to/notes.md \
                     --title "alpha0.0.3 — xxx"

assets_dir 里的每个文件都会作为资产上传；同名资产已存在会失败，先删再跑。
"""
import argparse
import json
import os
import sys
import urllib.error
import urllib.request

API = "https://api.github.com"
OWNER, REPO = "twipty", "ros-chinese-ime"


def token():
    with open(os.path.expanduser("~/mygithubpak/key.txt")) as f:
        return f.read().strip()


TOK = token()


def call(method, url, payload=None, raw=None, ctype="application/json"):
    data = raw if raw is not None else (
        json.dumps(payload).encode() if payload is not None else None)
    req = urllib.request.Request(url, data=data, method=method)
    req.add_header("Authorization", "Bearer " + TOK)
    req.add_header("Accept", "application/vnd.github+json")
    req.add_header("X-GitHub-Api-Version", "2022-11-28")
    if data:
        req.add_header("Content-Type", ctype)
    try:
        with urllib.request.urlopen(req, timeout=180) as r:
            body = r.read()
        return json.loads(body) if body else {}
    except urllib.error.HTTPError as e:
        print(f"HTTP {e.code} {method} {url}\n{e.read()[:400].decode(errors='replace')}",
              file=sys.stderr)
        raise


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tag")
    ap.add_argument("target_sha")
    ap.add_argument("assets_dir")
    ap.add_argument("notes_file")
    ap.add_argument("--title", default=None)
    ap.add_argument("--final", action="store_true",
                    help="标为正式版（默认 prerelease）")
    a = ap.parse_args()

    notes = open(a.notes_file, encoding="utf-8").read()
    title = a.title or a.tag

    tag_obj = call("POST", f"{API}/repos/{OWNER}/{REPO}/git/tags", {
        "tag": a.tag, "message": notes[:1000], "object": a.target_sha, "type": "commit",
        "tagger": {"name": "twipty",
                   "email": "82503737+twipty@users.noreply.github.com",
                   "date": "2026-11-18T00:00:00+08:00"}})
    print("tag object", tag_obj["sha"])
    try:
        call("POST", f"{API}/repos/{OWNER}/{REPO}/git/refs",
             {"ref": f"refs/tags/{a.tag}", "sha": tag_obj["sha"]})
        print("tag ref created")
    except Exception:
        print("tag ref 已存在，继续（release 会复用该 tag）")

    rel = call("POST", f"{API}/repos/{OWNER}/{REPO}/releases", {
        "tag_name": a.tag, "target_commitish": "main", "name": title,
        "body": notes, "draft": False, "prerelease": not a.final})
    print("release", rel["id"], rel["html_url"])

    upload = rel["upload_url"].split("{")[0]
    for name in sorted(os.listdir(a.assets_dir)):
        path = os.path.join(a.assets_dir, name)
        if not os.path.isfile(path) or name.endswith((".md", ".py")):
            continue
        with open(path, "rb") as f:
            data = f.read()
        asset = call("POST", f"{upload}?name={name}", raw=data,
                     ctype="application/octet-stream")
        print(f"asset {asset['name']} {asset['size']} {asset['state']}")


if __name__ == "__main__":
    main()
