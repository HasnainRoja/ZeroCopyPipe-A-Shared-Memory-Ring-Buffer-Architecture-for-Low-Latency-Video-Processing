import csv
import os
import sys
from datetime import datetime
from statistics import mean, stdev

ROOT = os.path.join(os.path.dirname(__file__), "..", "results")
ZC = os.path.join(ROOT, "latency_log.csv")
BL = os.path.join(ROOT, "baseline_log.csv")
OUT = os.path.join(ROOT, "summary.txt")
HIST = os.path.join(ROOT, "experiments_log.txt")

G = "\033[32m"
R = "\033[31m"
C = "\033[36m"
Y = "\033[33m"
B = "\033[1m"
Z = "\033[0m"


def load(path, col):
    vals = []
    try:
        with open(path, newline="") as f:
            for row in csv.DictReader(f):
                try:
                    v = float(row[col])
                    if v > 0:
                        vals.append(v)
                except Exception:
                    pass
    except FileNotFoundError:
        print(f"{R}Missing {path}{Z}")
    return vals


def pct(d, p):
    if not d:
        return 0.0
    s = sorted(d)
    k = (len(s) - 1) * p / 100.0
    f = int(k)
    c = min(f + 1, len(s) - 1)
    return s[f] + (s[c] - s[f]) * (k - f)


def stats(d):
    if not d:
        return dict(n=0, avg=0, min=0, max=0, p95=0, p99=0, std=0)
    return dict(
        n=len(d),
        avg=mean(d),
        min=min(d),
        max=max(d),
        p95=pct(d, 95),
        p99=pct(d, 99),
        std=stdev(d) if len(d) > 1 else 0.0,
    )


def ms(us):
    return f"{us / 1000:.3f} ms"


def main():
    os.makedirs(ROOT, exist_ok=True)

    mlock = os.environ.get("PIPE_MLOCK", "0")
    rt = os.environ.get("PIPE_RT", "0")
    mlock_s = "ON" if mlock == "1" else "OFF"
    rt_s = "ON" if rt == "1" else "OFF"

    print(f"\n{B}{C}{'=' * 60}")
    print("  SURGICAL VIDEO PIPELINE — BENCHMARK REPORT")
    print(f"{'=' * 60}{Z}\n")
    print(f"  Experiment: mlock={mlock_s} | SCHED_FIFO={rt_s}\n")

    zc_e2e = load(ZC, "end_to_end_us")
    bl_e2e = load(BL, "end_to_end_us")

    if not zc_e2e:
        print(f"{R}No zero-copy data. Run ./pipeline first.{Z}")
        sys.exit(1)

    zc = stats(zc_e2e)
    bl = stats(bl_e2e) if bl_e2e else None

    # Console: zero-copy
    print(f"{B}{G}ZERO-COPY ({zc['n']} frames){Z}")
    print("─" * 50)
    for k in ("avg", "min", "max", "p95", "p99", "std"):
        print(f"  {k.upper():<8} {ms(zc[k]):>12}")
    print("─" * 50)

    imp = spd = None
    if bl and bl["n"]:
        imp = ((bl["avg"] - zc["avg"]) / bl["avg"]) * 100.0
        spd = bl["avg"] / zc["avg"] if zc["avg"] else 0.0
        print(f"\n{B}COMPARISON{Z}")
        print("─" * 50)
        print(f"  {'Metric':<12} {'Zero-Copy':>12} {'Baseline':>12}")
        for lab, a, b in (
            ("Avg", zc["avg"], bl["avg"]),
            ("P99", zc["p99"], bl["p99"]),
            ("Max", zc["max"], bl["max"]),
        ):
            print(f"  {lab:<12} {ms(a):>12} {ms(b):>12}")
        print("─" * 50)
        col = G if imp > 0 else R
        print(f"\n  {col}{B}Improvement: {imp:+.1f}%{Z}")
        print(f"  {col}{B}Speedup    : {spd:.2f}x{Z}")

    ok = zc["p99"] < 70_000
    print(f"\n{B}CLINICAL CHECK (70 ms){Z}")
    print(f"  P99: {ms(zc['p99'])} → {G + 'PASS' + Z if ok else Y + 'REVIEW' + Z}")

    # Full summary.txt
    lines = []
    lines.append("SURGICAL VIDEO PIPELINE — BENCHMARK SUMMARY")
    lines.append("=" * 60)
    lines.append(f"Timestamp       : {datetime.now().isoformat(timespec='seconds')}")
    lines.append(f"Experiment      : mlock={mlock_s} | SCHED_FIFO={rt_s}")
    lines.append(f"Frame geometry  : 1920x1080 RGB")
    lines.append("")
    lines.append("ZERO-COPY PIPELINE")
    lines.append("-" * 40)
    lines.append(f"  Frames        : {zc['n']}")
    lines.append(f"  Avg latency   : {zc['avg'] / 1000:.3f} ms")
    lines.append(f"  Min latency   : {zc['min'] / 1000:.3f} ms")
    lines.append(f"  Max latency   : {zc['max'] / 1000:.3f} ms")
    lines.append(f"  P95 latency   : {zc['p95'] / 1000:.3f} ms")
    lines.append(f"  P99 latency   : {zc['p99'] / 1000:.3f} ms")
    lines.append(f"  Std deviation : {zc['std'] / 1000:.3f} ms")
    lines.append("")

    if bl and bl["n"]:
        lines.append("BASELINE (COPY) PIPELINE")
        lines.append("-" * 40)
        lines.append(f"  Frames        : {bl['n']}")
        lines.append(f"  Avg latency   : {bl['avg'] / 1000:.3f} ms")
        lines.append(f"  Min latency   : {bl['min'] / 1000:.3f} ms")
        lines.append(f"  Max latency   : {bl['max'] / 1000:.3f} ms")
        lines.append(f"  P95 latency   : {bl['p95'] / 1000:.3f} ms")
        lines.append(f"  P99 latency   : {bl['p99'] / 1000:.3f} ms")
        lines.append(f"  Std deviation : {bl['std'] / 1000:.3f} ms")
        lines.append("")
        lines.append("COMPARISON")
        lines.append("-" * 40)
        lines.append(f"  Improvement   : {imp:+.1f}%")
        lines.append(f"  Speedup       : {spd:.2f}x")
        lines.append("")

    lines.append("CLINICAL-STYLE THRESHOLD (70 ms)")
    lines.append("-" * 40)
    lines.append(f"  P99 latency   : {zc['p99'] / 1000:.3f} ms")
    lines.append(f"  Result        : {'PASS' if ok else 'REVIEW'}")
    lines.append("")
    lines.append("NOTES")
    lines.append("-" * 40)
    lines.append("  - Zero-copy path uses POSIX shared-memory ring buffer.")
    lines.append("  - Baseline uses full-frame memcpy between stages.")
    lines.append("  - mlock/SCHED_FIFO are optional OS predictability controls.")
    lines.append("  - If mlock fails without privilege, pipeline still runs correctly.")
    lines.append("")

    exp_row = (
        f"{datetime.now().isoformat(timespec='seconds')} | "
        f"mlock={mlock_s} | RT={rt_s} | "
        f"frames={zc['n']} | "
        f"avg_ms={zc['avg']/1000:.3f} | "
        f"p95_ms={zc['p95']/1000:.3f} | "
        f"p99_ms={zc['p99']/1000:.3f} | "
        f"max_ms={zc['max']/1000:.3f}"
    )
    if bl and bl["n"]:
        exp_row += f" | base_avg_ms={bl['avg']/1000:.3f} | speedup={spd:.2f}x | imp={imp:+.1f}%"
    lines.append("EXPERIMENT ROW")
    lines.append("-" * 40)
    lines.append(exp_row)
    lines.append("")

    with open(OUT, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

    with open(HIST, "a", encoding="utf-8") as f:
        f.write(exp_row + "\n")

    print(f"\n{G}Summary  → {OUT}{Z}")
    print(f"{G}History  → {HIST}{Z}\n")


if __name__ == "__main__":
    main()