#!/usr/bin/env python3
"""Exercise the real GNU make graphs with fake tools in an isolated SDK.

No compiler, SDK binary or make invocation runs in the source checkout. These
tests validate dependencies/publication, not firmware size, linking or hardware.
"""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest


SDK = Path(__file__).resolve().parents[1]
MCU = "offline_asr_llm_aiot_iis_sample"
BNPU = "bnpu_core_alg_pro"

FAKE_TOOL = r'''
import hashlib
import json
import os
from pathlib import Path
import sys

args = sys.argv[2:]
kind = sys.argv[1]
core = "bnpu" if "bnpu_core_alg_pro" in str(Path.cwd()) else "mcu"
event = {"kind": kind, "core": core, "args": args}
fd = os.open(os.environ["BUILD_EVENTS"], os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o600)
os.write(fd, (json.dumps(event) + "\n").encode())
os.close(fd)

def write(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)

def fail(stage):
    if os.environ.get("FAIL_STAGE") == core + ":" + stage:
        raise SystemExit(7)

if kind == "generate":
    source = "fixture/" + core + ".c"
    obj = "build/objs/" + core + ".o"
    Path("build/objs").mkdir(parents=True, exist_ok=True)
    Path("build/source_file.mk").write_text(
        "OBJS += " + obj + "\n"
        + obj + ": $(SDK_PATH)/" + source + "\n"
        + '\t$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"\n')
elif kind in ("compile", "link"):
    target = args[args.index("-o") + 1]
    if kind == "compile":
        content = Path(args[-1]).read_bytes()
    else:
        lds = Path(args[args.index("-T") + 1])
        content = lds.read_bytes()
        common = Path("../../../utils/common.lds").read_bytes()
        # Both linker scripts must use the same completed BNPU allocation.
        import re
        address = re.search(rb"SDK_ALG_PRO_SRAM_HOST_END_ADDR\s*=\s*(0x[0-9a-fA-F]+)", common)
        assert address and address.group(1).lower() in content.lower()
        for arg in args:
            if arg.endswith(".o"):
                content += Path(arg).read_bytes()
    # A failing linker/compiler deliberately leaves a partial target behind.
    write(target, b"PARTIAL")
    fail(kind)
    write(target, core.encode() + b":" + hashlib.sha256(content + repr(args).encode()).hexdigest().encode())
elif kind == "objcopy":
    write(args[-1], b"PARTIAL")
    fail(kind)
    write(args[-1], Path(args[-2]).read_bytes())
elif kind == "merge":
    directory = Path(args[args.index("-i") + 1])
    output = directory / (directory.name + ".bin")
    if os.environ.get("FAIL_STAGE") == "merge:empty":
        raise SystemExit(0)  # Real merge can return zero without an output.
    write(output, b"PARTIAL")
    if os.environ.get("FAIL_STAGE") == "merge:partial":
        raise SystemExit(8)
    write(output, b"MERGED:" + (directory / "[0]code.bin").read_bytes()
          + b"|" + (directory / "[1]code.bin").read_bytes())
elif kind == "listing":
    print("fake listing")
elif kind != "size":
    raise AssertionError(kind)
'''


class BuildGraphTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="lenwell-ci-build-dependency-")
        self.addCleanup(self.temp.cleanup)
        # Relative SDK_PATH must continue working beneath paths with spaces.
        self.root = Path(self.temp.name) / "sdk with spaces"
        self.root.mkdir()
        # Copy only text build inputs; never vendor or execute the real tools.
        inputs = [
            "system/user_alg_config.mk", "utils/common_head.mk",
            "utils/common_flags.mk", "utils/common_tail_mcu.mk",
            "utils/common_tail_bnpu.mk", "utils/common_temp.lds",
            "utils/generate_makefile.lua",
        ]
        for project, core in ((MCU, "mcu"), (BNPU, "bnpu")):
            inputs += [f"projects/{project}/project_file/makefile",
                       f"projects/{project}/lds/ci130x_asr_alg_{core}_temp.lds"]
        for relative in inputs:
            destination = self.root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(SDK / relative, destination)
        self.mcu = self.root / "projects" / MCU / "project_file"
        self.bnpu = self.root / "projects" / BNPU / "project_file"
        for project, core in ((self.mcu, "mcu"), (self.bnpu, "bnpu")):
            (project / "source_file.prj").write_text("fixture/" + core + ".c\n")
        (self.root / "fixture").mkdir()
        for core in ("mcu", "bnpu"):
            (self.root / "fixture" / (core + ".c")).write_text("source-" + core)
        (self.root / "libs").mkdir()
        for library in ("libcikd_pro_cwsl_use_frm.a", "libcikd_pro_cwsl_dis_frm.a"):
            (self.root / "libs" / library).write_bytes(b"fixture library")
        self.bnpu_image = self.root / "libs/ci130x_asr_alg_bnpu.a"
        self.bnpu_image.write_bytes(b"OLD VENDORED BNPU")
        self.final = self.mcu.parent / "firmware/user_code/user_code.bin"
        self.raw = self.final.parent / "[0]code.bin"
        self.tool = self.root / "fake_tool.py"
        self.tool.write_text(FAKE_TOOL)
        tools = self.root / "tools"
        tools.mkdir()
        merge = tools / "ci-tool-kit"
        merge.write_text("#!/bin/sh\nexec '" + sys.executable + "' '" + str(self.tool) + "' merge \"$@\"\n")
        merge.chmod(0o755)
        self.events_path = self.root / "events.jsonl"
        self.env = os.environ.copy()
        self.env.pop("MAKEFLAGS", None)
        self.env.pop("MFLAGS", None)
        self.env.pop("MAKELEVEL", None)
        self.env["BUILD_EVENTS"] = str(self.events_path)
        self.make = shutil.which("make")
        self.assertIsNotNone(self.make, "GNU make is required")
        self.options = [
            "OS=Windows_NT", "SHELL=/bin/bash", "CMDBOX=", "CC_PREFIX=",
            "PROJECT_NAME=" + MCU,
        ]
        for variable, kind in (("CC", "compile"), ("AS", "compile"),
                               ("LD", "link"), ("OC", "objcopy"),
                               ("OD", "listing"), ("SIZE", "size"),
                               ("LUA", "generate")):
            self.options.append(f"{variable}='{sys.executable}' '{self.tool}' {kind}")

    def run_make(self, *goals, extra=(), fail=None, success=True, directory=None):
        # macOS ships GNU make 3.81 with second-resolution timestamp checks.
        # Fake compilers are instantaneous; real firmware builds are not.
        time.sleep(1.05)
        self.events_path.write_text("")
        env = self.env.copy()
        if fail:
            env["FAIL_STAGE"] = fail
        result = subprocess.run(
            [self.make, "-r", "-j8", *self.options, *extra, *goals],
            cwd=directory or self.mcu, env=env, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30,
        )
        if success:
            self.assertEqual(result.returncode, 0, result.stdout)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
        events = [json.loads(line) for line in self.events_path.read_text().splitlines()]
        return events, result.stdout

    def change(self, relative, content="\n/* changed */\n"):
        # Also works with older make/filesystem timestamp granularity.
        time.sleep(1.05)
        with (self.root / relative).open("a") as stream:
            stream.write(content)

    @staticmethod
    def work(events):
        return [(event["core"], event["kind"]) for event in events
                if event["kind"] not in ("listing", "size", "generate")]

    def assert_both_cores_built(self, events):
        work = self.work(events)
        self.assertIn(("bnpu", "compile"), work)
        self.assertIn(("mcu", "compile"), work)
        self.assertLess(work.index(("bnpu", "objcopy")), work.index(("mcu", "compile")))
        self.assertLess(work.index(("mcu", "objcopy")), work.index(("mcu", "merge")))

    def test_first_build_and_noop(self):
        events, output = self.run_make(extra=("SDK_PATH=../../..",))
        self.assert_both_cores_built(events)
        self.assertNotIn("jobserver unavailable", output)
        self.assertEqual(self.final.read_bytes(), b"MERGED:" + self.raw.read_bytes()
                         + b"|" + self.bnpu_image.read_bytes())
        self.assertNotIn(b"OLD VENDORED BNPU", self.final.read_bytes())
        # Explicit parent PROJECT_NAME must not leak into the BNPU ELF name.
        self.assertTrue((self.bnpu / "build" / (BNPU + ".elf")).exists())
        self.assertFalse((self.bnpu / "build" / (MCU + ".elf")).exists())
        events, _ = self.run_make("all", "secondary-outputs", "user_code")
        self.assertEqual(events, [])

    def test_ide_environment_project_name_does_not_leak_to_bnpu(self):
        self.options.remove("PROJECT_NAME=" + MCU)
        self.env["PROJECT_NAME"] = MCU
        events, _ = self.run_make()
        self.assert_both_cores_built(events)
        self.assertTrue((self.bnpu / "build" / (BNPU + ".elf")).exists())
        self.assertTrue((self.mcu / "build" / (MCU + ".elf")).exists())

    def test_bnpu_source_only_remerges_without_mcu_compile(self):
        self.run_make()
        before = self.final.read_bytes()
        self.change("fixture/bnpu.c")
        events, _ = self.run_make("secondary-outputs")
        self.assertIn(("bnpu", "compile"), self.work(events))
        self.assertIn(("mcu", "merge"), self.work(events))
        self.assertNotIn(("mcu", "compile"), self.work(events))
        self.assertNotEqual(before, self.final.read_bytes())

    def test_configuration_and_effective_flags_invalidate_objects_and_lds(self):
        self.run_make()
        common = self.root / "utils/common.lds"
        before = common.read_bytes()
        self.change("system/user_alg_config.mk", "\nAEC_USE_BNPU_SRAM_SIZE = 28*1024\n")
        events, _ = self.run_make()
        self.assert_both_cores_built(events)
        self.assertNotEqual(before, common.read_bytes())
        mcu_lds = self.mcu.parent / "lds/ci130x_asr_alg_mcu.lds"
        before = mcu_lds.stat().st_mtime_ns
        events, _ = self.run_make(extra=("O_OPTION=-O1",))
        self.assert_both_cores_built(events)
        self.assertGreater(mcu_lds.stat().st_mtime_ns, before)
        events, _ = self.run_make(extra=("O_OPTION=-O1",))
        self.assertEqual(events, [])

    def test_missing_common_lds_precedes_mcu_and_explicit_elf(self):
        self.run_make()
        (self.root / "utils/common.lds").unlink()
        events, _ = self.run_make("build/" + MCU + ".elf")
        work = self.work(events)
        self.assertIn(("bnpu", "link"), work)
        self.assertIn(("mcu", "link"), work)
        self.assertLess(work.index(("bnpu", "objcopy")), work.index(("mcu", "link")))
        self.assertNotIn(("mcu", "merge"), work)
        events, _ = self.run_make("../firmware/user_code/user_code.bin")
        self.assertEqual(self.work(events), [("mcu", "objcopy"), ("mcu", "merge")])

    def test_config_source_change_and_new_stamp_invalidate_old_cache(self):
        self.run_make()
        self.change("utils/common_head.mk", "\n# build configuration updated\n")
        events, _ = self.run_make()
        self.assert_both_cores_built(events)
        (self.bnpu / "build/build-config.txt").unlink()
        events, _ = self.run_make()
        self.assertIn(("bnpu", "compile"), self.work(events))
        self.assertIn(("mcu", "merge"), self.work(events))

    def test_bnpu_failure_blocks_mcu_and_preserves_package(self):
        self.run_make()
        before = self.final.read_bytes()
        self.change("fixture/bnpu.c")
        events, _ = self.run_make(fail="bnpu:link", success=False)
        self.assertFalse(any(event["core"] == "mcu" for event in events))
        self.assertEqual(before, self.final.read_bytes())
        self.assertFalse((self.bnpu / "build" / (BNPU + ".elf")).exists())
        events, _ = self.run_make()
        self.assertIn(("bnpu", "link"), self.work(events))
        self.assertIn(("mcu", "merge"), self.work(events))

    def test_failed_objcopy_does_not_publish_partial_images(self):
        self.run_make()
        for core, project, image in (("bnpu", BNPU, self.bnpu_image),
                                     ("mcu", MCU, self.raw)):
            before = image.read_bytes()
            package = self.final.read_bytes()
            self.change("fixture/" + core + ".c")
            events, _ = self.run_make(fail=core + ":objcopy", success=False)
            self.assertEqual(before, image.read_bytes())
            self.assertEqual(package, self.final.read_bytes())
            self.assertFalse(Path(str(image) + ".tmp").exists())
            self.assertNotIn(("mcu", "merge"), self.work(events))
            events, _ = self.run_make()
            self.assertIn((core, "objcopy"), self.work(events))
            self.assertIn(("mcu", "merge"), self.work(events))

    def test_merge_failure_and_empty_success_keep_old_package(self):
        self.run_make()
        for failure in ("merge:partial", "merge:empty"):
            before = self.final.read_bytes()
            self.change("fixture/bnpu.c")
            self.run_make(fail=failure, success=False)
            self.assertEqual(before, self.final.read_bytes())
            self.assertEqual(list((self.mcu / "build/merge/user_code").iterdir()), [])
            events, _ = self.run_make("user_code")
            self.assertEqual(self.work(events), [("mcu", "merge")])
            self.assertNotEqual(before, self.final.read_bytes())

    def test_clean_does_not_generate_sources_or_build_bnpu(self):
        events, _ = self.run_make("clean")
        self.assertEqual(events, [])
        self.assertFalse((self.bnpu / "build").exists())
        self.assertFalse((self.mcu / "build/source_file.mk").exists())
        # Direct algorithm-project clean is also safe without generated inputs.
        events, _ = self.run_make("clean", directory=self.bnpu,
                                  extra=("PROJECT_NAME=" + BNPU,))
        self.assertEqual(events, [])
        events, _ = self.run_make("clean", "all", success=False)
        self.assertEqual(events, [])


if __name__ == "__main__":
    unittest.main(verbosity=2)
