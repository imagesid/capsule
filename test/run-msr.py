#!/usr/bin/env python3
import subprocess, re, csv, sys, os, glob, shlex

# === CONFIG ===
DATA_DIR = f"./data/msr/"
print(DATA_DIR)
BIN = "./_build/bin/cachesim"
WORKDIR = "./"
LOG_DIR = "logs"
os.makedirs(LOG_DIR, exist_ok=True)

# === FILES ===
files = sorted(glob.glob(os.path.join(DATA_DIR, "*.zst")))
if not files:
    print(" No files found in DATA_DIR.")
    sys.exit(1)

print(f"Found {len(files)} trace files")
for f in files:
    print("  -", os.path.basename(f))

# === RUNNER ===
def run_cachesim(path, algo, *extra):
    cmd = [BIN, path, "oracleGeneral", algo, "256MB", *extra]
    print("→", " ".join(shlex.quote(c) for c in cmd))

    try:
        out = subprocess.run(
            cmd,
            cwd=WORKDIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=60000,
        )

        if out.returncode != 0:
            print(f"   Exit code {out.returncode}")
            if out.stderr.strip():
                print("   stderr:", out.stderr.strip())

        matches = re.findall(r"miss ratio ([0-9.]+)", out.stdout)
        if matches:
            val = matches[-1]
            print(f"   Parsed miss ratio: {val}")
            return val
        else:
            print("   - No 'miss ratio' found; output tail:")
            print("\n".join(out.stdout.splitlines()[-5:]))
            return "N/A"

    except Exception as e:
        print("   Exception:", e)
        return "ERR"

# === OUTPUT CSV ===
out_csv = os.path.join(LOG_DIR, f"result-msr.csv")
header = [
    "Filename","LRU","LFU","ARC","Belady","CACHEUS",
    "PG","PG_CAPSULE","OBL","OBL_CAPSULE",
    "MITHRIL","MITHRIL_CAPSULE","CAPSULE"
]

print(f"\nWriting results to {out_csv}")

with open(out_csv, "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(header)

    for idx, path in enumerate(files, 1):
        fname = os.path.basename(path)
        print(f"\n[{idx}/{len(files)}] Processing {fname}")

        results = [
            os.path.splitext(fname)[0],
            run_cachesim(path, "lru"),
            run_cachesim(path, "lfu"),
            run_cachesim(path, "arc"),
            run_cachesim(path, "belady"),
            run_cachesim(path, "cacheus"),
            run_cachesim(path, "lru", "--prefetch=PG"),
            run_cachesim(path, "lru", "--prefetch=PGCluster"),
            run_cachesim(path, "lru", "--prefetch=OBL"),
            run_cachesim(path, "lru", "--prefetch=OBLCluster"),
            run_cachesim(path, "lru", "--prefetch=Mithril"),
            run_cachesim(path, "lru", "--prefetch=MithrilCluster"),
            run_cachesim(path, "lru", "--prefetch=CAPSULE"),
        ]

        writer.writerow(results)
        f.flush()

print(f"\n All runs complete — results saved to {out_csv}")
