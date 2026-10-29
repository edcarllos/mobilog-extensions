#!/usr/bin/env python3
"""Checks that each package in mobilog_store/ is the build of its sources: builds every extension again
(unsigned) and compares every member of the package, except signature.json, with the store's one.
    scripts/drift.py [mobilog_ext]
"""
import json, os, subprocess, sys, zipfile

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
tool = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "build/tools/mobilog_ext/mobilog_ext"))
store = os.path.join(root, "mobilog_store")
bad = 0
for name in sorted(os.listdir(os.path.join(root, "extensions"))):
    folder = os.path.join(root, "extensions", name)
    manifest = os.path.join(folder, "manifest.json")
    if not os.path.isfile(manifest):
        continue
    v = json.load(open(manifest))["version"]
    published = os.path.join(store, f"{name}-{v}.mlx")
    if not os.path.isfile(published):
        print(f"{name} {v}: ainda não publicada / not published yet")
        continue
    subprocess.run([tool, "build", folder, "-q"], check=True)
    built = zipfile.ZipFile(os.path.join(folder, "dist", f"{name}-{v}.mlx"))
    pub = zipfile.ZipFile(published)
    a = {n: built.read(n) for n in built.namelist()}
    b = {n: pub.read(n) for n in pub.namelist() if n != "signature.json"}
    if a != b:
        diff = sorted(set(a) ^ set(b)) + sorted(n for n in set(a) & set(b) if a[n] != b[n])
        print(f"{name} {v}: DIFERE do fonte / DIFFERS from its source: {', '.join(diff)}")
        bad += 1
    else:
        print(f"{name} {v}: confere / matches")
sys.exit(1 if bad else 0)
