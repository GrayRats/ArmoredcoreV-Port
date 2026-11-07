import sys as _sys
if _sys.version_info < (3, 15):
    raise SystemExit('requires Python 3.15+; run: uv run --no-project --python 3.15 python -I <script>')
import subprocess, sys, unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools" / "llmignore.py"


def check(*paths):
    out = subprocess.run([sys.executable, "-I", str(TOOL), "check", "--", *paths], capture_output=True, text=True)
    return out.returncode, {line.split()[1]: line.split()[0] for line in out.stdout.splitlines()}


class LlmIgnoreTests(unittest.TestCase):
    def test_scratch_and_data_ignored(self):
        code, result = check("tmp/a/b.log", "logs/run/runtime.log", "assets/x", "x/y/lib.so", "--callers")
        self.assertEqual(code, 1)
        self.assertEqual(set(result.values()), {"ignored"})

    def test_sources_and_docs_allowed(self):
        code, result = check("tools/llmignore.py", "tools/acv-context/kb.py", "tools/rexglue-sdk/src/ui/window_sdl.cpp",
                             "tools/native-rexglue/probe_linux.py", "TOOLS.MD")
        self.assertEqual(code, 0)
        self.assertEqual(set(result.values()), {"allowed"})

    def test_nested_build_tree_ignored(self):
        _, result = check("tools/rexglue-sdk/out/build/x.cpp", "tools/foo/node_modules/a.js")
        self.assertEqual(set(result.values()), {"ignored"})

    def test_repo_gitignore_does_not_leak(self):
        # the knowledge-base database is read through kb.py only; .llmignore alone decides.
        _, result = check("tools/acv-context/knowledge.sqlite3", "scripts/x.bin")
        self.assertEqual(result["tools/acv-context/knowledge.sqlite3"], "ignored")
        self.assertEqual(result["scripts/x.bin"], "allowed")


if __name__ == "__main__":
    unittest.main()
