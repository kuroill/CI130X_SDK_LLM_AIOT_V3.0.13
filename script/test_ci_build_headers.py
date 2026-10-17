#!/usr/bin/env python3
"""Check actual MCU/BNPU include paths with the host C preprocessor.

No SDK compiler, firmware link or package generation is executed.
"""

from pathlib import Path
import shutil
import subprocess
import unittest


SDK = Path(__file__).resolve().parents[1]
MCU = "offline_asr_llm_aiot_iis_sample"
BNPU = "bnpu_core_alg_pro"


class BuildHeadersTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler = shutil.which("cc") or shutil.which("clang") or shutil.which("gcc")
        if cls.compiler is None:
            raise unittest.SkipTest("a host C preprocessor is required")

    def include_paths(self, project):
        source_list = SDK / "projects" / project / "project_file/source_file.prj"
        paths = []
        for line in source_list.read_text().splitlines():
            if line.strip().startswith("include-path:"):
                paths.append(SDK / line.split(":", 1)[1].strip())
        self.assertTrue(paths, f"missing include paths for {project}")
        return paths

    def preprocess(self, project, source):
        includes = []
        for path in self.include_paths(project):
            includes.extend(("-I", str(path)))
        result = subprocess.run(
            [self.compiler, "-E", "-P", "-Werror", "-x", "c", *includes,
             '-DCI_CONFIG_FILE="user_config.h"', "-"],
            input=source, text=True, capture_output=True, timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def check_audio_header(self, project):
        self.preprocess(project, '''
#include "ci_conversation.generated.h"
#if CI_AUDIO_SAMPLE_RATE_HZ != 16000 || CI_AUDIO_DATA_BITS != 16 || \
    CI_AUDIO_SLOT_BITS != 32 || CI_AUDIO_CHANNELS != 2 || CI_AUDIO_BCLK_PER_FRAME != 64
#error CI audio format changed
#endif
''')
        header = "ci_conversation.generated.h"
        resolved = next((p / header for p in self.include_paths(project)
                         if (p / header).is_file()), None)
        self.assertEqual(resolved, SDK / "components/ci_common" / header)

    def test_mcu_resolves_shared_audio_header(self):
        self.check_audio_header(MCU)

    def test_bnpu_resolves_shared_audio_header(self):
        self.check_audio_header(BNPU)

    def check_bnpu_config(self, first_header):
        self.preprocess(BNPU, f'''
#include "{first_header}"
#if USE_PWK != 1
#error BNPU PWK policy changed
#endif
#if CONFIG_CI_LOG_UART != 0
#error BNPU logging policy changed
#endif
''')

    def test_bnpu_config_before_sdk_defaults(self):
        self.check_bnpu_config("user_config.h")

    def test_bnpu_config_after_sdk_defaults(self):
        self.check_bnpu_config("sdk_default_config.h")


if __name__ == "__main__":
    unittest.main(verbosity=2)
