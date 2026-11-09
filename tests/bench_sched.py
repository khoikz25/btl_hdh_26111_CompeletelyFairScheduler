#!/usr/bin/env python3
"""Do muc do chia se CPU cua scheduler xv6 bang user/schedtest (Task 6).

Moi lan do: khoi dong QEMU moi, go "schedtest N TICKS" trong shell xv6,
doc ket qua, roi tat QEMU. Lap lai REPS lan cho moi cap (CPUS, N) de lay
trung binh va do lech chuan (de bai yeu cau it nhat 5 lan).

Cach dung (chay o thu muc goc cua repo, sau khi kernel da build duoc):

  # do voi kernel hien tai, dat nhan de phan biet (vd: stock hoac cfs)
  python3 tests/bench_sched.py run --label stock --out results/stock.csv

  # tuy chinh
  python3 tests/bench_sched.py run --label cfs --out results/cfs.csv \
      --reps 5 --ticks 100 --cpus 1 4 --ns 1 2 4 8

  # so sanh hai ket qua
  python3 tests/bench_sched.py compare results/stock.csv results/cfs.csv

Moi lan do in PASS hoac FAIL. Thoat voi ma 1 neu co lan nao FAIL.
Chi tieu do cho moi lan:
  total  : tong so vong lap cua tat ca tien trinh (thong luong)
  ratio  : max/min vong lap giua cac tien trinh (1.000 la deu tuyet doi)
  jain   : chi so cong bang Jain, (sum x)^2 / (n * sum x^2), 1.0 la deu
"""

import argparse
import csv
import os
import re
import select
import signal
import statistics
import subprocess
import sys
import time

BOOT_RE = r"init: starting sh"
PROC_RE = re.compile(r"^proc (\d+) pid (\d+) count (\d+) share ([\d.]+)%")
TOTAL_RE = re.compile(r"^total (\d+) min (\d+) max (\d+)")
TOTAL_WAIT_RE = r"^total \d+ min \d+ max \d+\r?\n"
FIELDS = [
    "label", "cpus", "n", "ticks", "rep", "status",
    "total", "min", "max", "ratio", "jain", "counts",
]


class Qemu:
    """Chay 'make qemu CPUS=k' va doc/ghi qua stdin, stdout."""

    def __init__(self, cpus):
        self.proc = subprocess.Popen(
            ["make", "qemu", "CPUS=%d" % cpus],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            start_new_session=True,  # de giet ca nhom tien trinh make + qemu
        )
        self.fd = self.proc.stdout.fileno()
        os.set_blocking(self.fd, False)
        self.buf = ""

    def _pump(self, timeout):
        """Doc them du lieu. Tra ve False neu QEMU da thoat."""
        ready, _, _ = select.select([self.fd], [], [], timeout)
        if not ready:
            return True
        try:
            data = os.read(self.fd, 4096)
        except BlockingIOError:
            return True
        if not data:
            return False
        self.buf += data.decode("utf-8", "replace")
        return True

    def wait_for(self, regex, timeout, start=0):
        pat = re.compile(regex, re.M)
        deadline = time.time() + timeout
        alive = True
        while time.time() < deadline and alive:
            m = pat.search(self.buf, start)
            if m:
                return m
            alive = self._pump(0.5)
        return pat.search(self.buf, start)

    def send(self, text):
        self.proc.stdin.write(text.encode("utf-8"))
        self.proc.stdin.flush()

    def stop(self):
        try:
            os.killpg(self.proc.pid, signal.SIGKILL)
        except (ProcessLookupError, PermissionError):
            pass
        try:
            self.proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            pass


def parse_schedtest(text):
    """Tach ket qua schedtest. Tra ve (counts, total, min, max) hoac None."""
    counts = {}
    total = None
    for line in text.splitlines():
        m = PROC_RE.match(line)
        if m:
            counts[int(m.group(1))] = int(m.group(3))
            continue
        m = TOTAL_RE.match(line)
        if m:
            total = (int(m.group(1)), int(m.group(2)), int(m.group(3)))
    if total is None:
        return None
    ordered = [counts[k] for k in sorted(counts)]
    return ordered, total[0], total[1], total[2]


def metrics(counts):
    """ratio = max/min, jain = chi so cong bang Jain."""
    lo, hi = min(counts), max(counts)
    ratio = hi / lo if lo > 0 else float("inf")
    s = sum(counts)
    sq = sum(c * c for c in counts)
    jain = (s * s) / (len(counts) * sq) if sq > 0 else 0.0
    return ratio, jain


def check_run(n, parsed):
    """Kiem tra ket qua hop le. Tra ve chuoi loi, hoac '' neu dung."""
    if parsed is None:
        return "khong thay dong 'total ...' trong ket qua"
    counts, total, lo, hi = parsed
    if len(counts) != n:
        return "can %d tien trinh nhung chi doc duoc %d" % (n, len(counts))
    if min(counts) <= 0:
        return "co tien trinh khong chay duoc vong lap nao (count = 0)"
    if sum(counts) != total:
        return "tong cac count (%d) khac total (%d)" % (sum(counts), total)
    if lo != min(counts) or hi != max(counts):
        return "min/max khong khop voi danh sach count"
    return ""


def run_once(cpus, n, ticks, boot_timeout=90):
    """Mot lan do. Tra ve (parsed, loi)."""
    q = Qemu(cpus)
    try:
        if not q.wait_for(BOOT_RE, boot_timeout):
            return None, "xv6 khong khoi dong duoc (khong thay 'init: starting sh')"
        mark = len(q.buf)
        q.send("schedtest %d %d\n" % (n, ticks))
        run_timeout = max(60, ticks * 0.5 + 30)
        if not q.wait_for(TOTAL_WAIT_RE, run_timeout, start=mark):
            return None, "het thoi gian cho ket qua schedtest (%ds)" % run_timeout
        return parse_schedtest(q.buf[mark:]), ""
    finally:
        q.stop()


def build():
    print("== Build kernel va fs.img ==")
    subprocess.run(["make", "kernel/kernel", "fs.img"], check=True)


def cmd_run(args):
    if not args.no_build:
        build()
    out_dir = os.path.dirname(args.out)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    new_file = not os.path.exists(args.out)
    failures = 0
    with open(args.out, "a", newline="") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS, lineterminator="\n")
        if new_file:
            w.writeheader()
        for cpus in args.cpus:
            for n in args.ns:
                for rep in range(1, args.reps + 1):
                    parsed, err = run_once(cpus, n, args.ticks)
                    if not err:
                        err = check_run(n, parsed)
                    tag = "cpus=%d n=%d rep=%d/%d" % (cpus, n, rep, args.reps)
                    if err:
                        failures += 1
                        print("[FAIL] %s: %s" % (tag, err), flush=True)
                        w.writerow({"label": args.label, "cpus": cpus, "n": n,
                                    "ticks": args.ticks, "rep": rep,
                                    "status": "FAIL"})
                        f.flush()
                        continue
                    counts, total, lo, hi = parsed
                    ratio, jain = metrics(counts)
                    print("[PASS] %s total=%d max/min=%.3f jain=%.4f"
                          % (tag, total, ratio, jain), flush=True)
                    w.writerow({"label": args.label, "cpus": cpus, "n": n,
                                "ticks": args.ticks, "rep": rep,
                                "status": "PASS", "total": total, "min": lo,
                                "max": hi, "ratio": "%.6f" % ratio,
                                "jain": "%.6f" % jain,
                                "counts": " ".join(map(str, counts))})
                    f.flush()
    rows = read_rows(args.out, args.label)
    print_summary(rows, args.label)
    if failures:
        print("FAIL: %d lan do khong dat" % failures)
        return 1
    print("PASS: moi lan do deu hop le")
    return 0


def read_rows(path, label=None):
    rows = []
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            if label is None or r["label"] == label:
                rows.append(r)
    return rows


def summarize(rows):
    """Gom theo (cpus, n): trung binh va do lech chuan cua cac lan PASS."""
    groups = {}
    for r in rows:
        if r["status"] != "PASS":
            continue
        key = (int(r["cpus"]), int(r["n"]))
        groups.setdefault(key, []).append(r)
    stats = {}
    for key, rs in groups.items():
        def col(name):
            return [float(x[name]) for x in rs]

        def mean_sd(v):
            return statistics.mean(v), (statistics.stdev(v) if len(v) > 1 else 0.0)

        stats[key] = {
            "runs": len(rs),
            "total": mean_sd(col("total")),
            "ratio": mean_sd(col("ratio")),
            "jain": mean_sd(col("jain")),
        }
    return stats


def print_summary(rows, label):
    stats = summarize(rows)
    print("\n== Tong ket '%s' (trung binh +- do lech chuan) ==" % label)
    print("%-5s %-3s %-5s %-22s %-18s %-16s"
          % ("CPUS", "N", "runs", "total", "max/min", "jain"))
    for (cpus, n) in sorted(stats):
        s = stats[(cpus, n)]
        print("%-5d %-3d %-5d %-22s %-18s %-16s" % (
            cpus, n, s["runs"],
            "%.0f +- %.0f" % s["total"],
            "%.3f +- %.3f" % s["ratio"],
            "%.4f +- %.4f" % s["jain"]))


def cmd_compare(args):
    a = summarize(read_rows(args.a))
    b = summarize(read_rows(args.b))
    print("A = %s, B = %s" % (args.a, args.b))
    print("%-5s %-3s | %-19s | %-19s | %-14s"
          % ("CPUS", "N", "max/min A -> B", "jain A -> B", "total B vs A"))
    for key in sorted(set(a) & set(b)):
        cpus, n = key
        ta, tb = a[key]["total"][0], b[key]["total"][0]
        diff = (tb - ta) / ta * 100.0 if ta else float("nan")
        print("%-5d %-3d | %7.3f -> %-9.3f | %7.4f -> %-9.4f | %+.1f%%" % (
            cpus, n,
            a[key]["ratio"][0], b[key]["ratio"][0],
            a[key]["jain"][0], b[key]["jain"][0], diff))
    only = sorted(set(a) ^ set(b))
    if only:
        print("Luu y: cac cap (CPUS, N) chi co o mot ben: %s" % only)
    return 0


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawTextHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    r = sub.add_parser("run", help="do voi kernel hien tai")
    r.add_argument("--label", required=True, help="nhan, vd stock hoac cfs")
    r.add_argument("--out", required=True, help="file CSV ket qua (ghi noi tiep)")
    r.add_argument("--reps", type=int, default=5, help="so lan lap (mac dinh 5)")
    r.add_argument("--ticks", type=int, default=100, help="thoi gian moi lan do")
    r.add_argument("--cpus", type=int, nargs="+", default=[1, 4])
    r.add_argument("--ns", type=int, nargs="+", default=[1, 2, 4, 8])
    r.add_argument("--no-build", action="store_true", help="bo qua buoc make")
    r.set_defaults(func=cmd_run)

    c = sub.add_parser("compare", help="so sanh hai file CSV")
    c.add_argument("a")
    c.add_argument("b")
    c.set_defaults(func=cmd_compare)

    args = p.parse_args()
    sys.exit(args.func(args))


if __name__ == "__main__":
    main()