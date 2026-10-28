"""Run isolated runtime spikes and print their measured limits."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
BUILD = ROOT / "build/runtimes"
MODULE = BUILD / "spike.wasm"
for runtime in ("lua", "wasm3", "wamr"):
    binary = BUILD / "spikes" / (runtime + "-spike.exe")
    args = [str(binary)] + ([] if runtime == "lua" else [str(MODULE)])
    run = subprocess.run(args, capture_output=True, text=True, timeout=5)
    (BUILD / (runtime + "-result.log")).write_text(run.stdout + run.stderr, encoding="utf-8")
    print(run.stdout, end="")
    print(run.stderr, end="")
    if run.returncode:
        raise SystemExit(run.returncode)
    print(runtime, "executable_bytes=" + str(binary.stat().st_size))
try:
    run = subprocess.run([str(BUILD / "spikes/wasm3-spike.exe"), str(MODULE), "spin"],
                         capture_output=True, text=True, timeout=1)
except subprocess.TimeoutExpired:
    print("wasm3 loop_stop=unsupported process_timeout_ms=1000")
else:
    raise SystemExit("The wasm3 endless-loop probe did not reach its expected timeout.")
