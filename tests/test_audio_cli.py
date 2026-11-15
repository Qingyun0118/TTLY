"""Exercise audio CLI routing without importing ML packages or loading model files."""

import contextlib
import importlib.util
import io
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import wave


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "Sound_Rec" / "soundimprove.py"
spec = importlib.util.spec_from_file_location("sound_cli_under_test", SCRIPT)
sound_cli = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sound_cli)


class AudioCliTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="ttly-audio-test-")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.environment = patch.dict(os.environ, {}, clear=False)
        self.environment.start()
        self.addCleanup(self.environment.stop)
        self.model = self.directory / "model.joblib3"
        self.good = self.directory / "good.wav"
        self.bad = self.directory / "bad.wav"
        for path in (self.model, self.good, self.bad):
            path.touch()
        self.yamnet = self.directory / "yamnet"
        self.yamnet.mkdir()
        self.common = ["--model", str(self.model), "--yamnet", str(self.yamnet),
                       "--cache-dir", str(self.directory / "cache")]
        self.events = []
        events = self.events

        class StubClassifier:
            def __init__(self, yamnet):
                events.append(("init", yamnet))

            def load_model(self, path):
                events.append(("load", path))

            def train(self, **kwargs):
                events.append(("train", kwargs))

            def predict_long_audio(self, path):
                if path.name == "bad.wav":
                    raise ValueError("stub prediction failure")
                return [("Harp Seal", 0.95)]

        self.stub = StubClassifier

    def invoke(self, args):
        output, error = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(error):
            try:
                code = sound_cli.main(args)
            except SystemExit as exc:
                code = exc.code
        return code, output.getvalue(), error.getvalue()

    def test_help_without_site_packages(self):
        result = subprocess.run(
            [sys.executable, "-S", "-B", str(SCRIPT), "--help"],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("--yamnet", result.stdout)
        self.assertIn("--train", result.stdout)
        self.assertEqual(self.invoke([])[0], 2)

    def test_explicit_model_and_qt_output(self):
        with patch.object(sound_cli, "MarineAnimalClassifier", self.stub):
            code, output, error = self.invoke(self.common + ["--audio", str(self.good)])
        self.assertEqual(code, 0, error)
        self.assertIn("- Harp Seal (0.95)", output)
        self.assertIn(("load", self.model), self.events)
        self.assertFalse(any(event[0] == "train" for event in self.events))

    def test_default_model_is_independent_of_working_directory(self):
        previous = Path.cwd()
        try:
            os.chdir(self.directory)
            with patch.object(sound_cli, "MarineAnimalClassifier", self.stub):
                code, _, error = self.invoke([
                    "--audio", str(self.good), "--yamnet", str(self.yamnet),
                    "--cache-dir", str(self.directory / "cache"),
                ])
        finally:
            os.chdir(previous)
        self.assertEqual(code, 0, error)
        self.assertIn(("load", SCRIPT.parent / "animal_classifier_pro.joblib3"), self.events)

    def test_missing_files_and_prediction_failure_return_nonzero(self):
        missing = self.directory / "missing.wav"
        with patch.object(sound_cli, "MarineAnimalClassifier", self.stub):
            self.assertEqual(self.invoke(self.common + ["--audio", str(missing)])[0], 1)
            code, output, error = self.invoke(
                self.common + ["--audio", str(self.good), "--audio", str(missing)])
            self.assertEqual(code, 1)
            self.assertIn("- Harp Seal (0.95)", output)
            self.assertIn(str(missing), error)
            self.assertEqual(self.invoke(self.common + ["--audio", str(self.bad)])[0], 1)
            code, output, error = self.invoke(
                self.common + ["--audio", str(self.bad), "--audio", str(self.good)])
            self.assertEqual(code, 1)
            self.assertIn("stub prediction failure", error)
            self.assertIn("- Harp Seal (0.95)", output)
        self.assertEqual(self.invoke([
            "--model", str(self.directory / "missing-model"), "--audio", str(self.good),
        ])[0], 1)

    def test_model_loading_and_dependency_failures_return_nonzero(self):
        class BrokenLoad(self.stub):
            def load_model(self, path):
                raise ValueError("stub invalid model")

        with patch.object(sound_cli, "MarineAnimalClassifier", BrokenLoad):
            self.assertEqual(self.invoke(self.common + ["--audio", str(self.good)])[0], 1)
        with patch.object(sound_cli, "MarineAnimalClassifier", side_effect=ImportError("missing dependency")):
            self.assertEqual(self.invoke(self.common + ["--audio", str(self.good)])[0], 1)

    def test_training_is_explicit_and_accepts_external_data(self):
        data = self.directory / "training-data"
        data.mkdir()
        with patch.object(sound_cli, "MarineAnimalClassifier", self.stub):
            code, _, error = self.invoke(self.common + ["--train", "--data-dir", str(data)])
        self.assertEqual(code, 0, error)
        self.assertEqual(self.events[-1], ("train", {"data_dir": data, "model_path": self.model}))
        self.assertFalse(any(event[0] == "load" for event in self.events))

    def test_environment_supplies_offline_yamnet_and_cache(self):
        cache = self.directory / "environment-cache"
        os.environ["YAMNET_MODEL"] = str(self.yamnet)
        os.environ["TFHUB_CACHE_DIR"] = str(cache)
        with patch.object(sound_cli, "MarineAnimalClassifier", self.stub):
            code, _, error = self.invoke(["--model", str(self.model), "--audio", str(self.good)])
        self.assertEqual(code, 0, error)
        self.assertIn(("init", str(self.yamnet)), self.events)
        self.assertTrue(cache.is_dir())

    def test_wav_tools_round_trip(self):
        original = self.directory / "original.wav"
        with wave.open(str(original), "wb") as audio:
            audio.setparams((1, 2, 8000, 0, "NONE", "not compressed"))
            audio.writeframes(b"\x01\x00" * 16000)
        parts = self.directory / "parts"
        cut = subprocess.run([
            sys.executable, "-B", str(SCRIPT.parent / "cut.py"), str(original), "1",
            "--output-dir", str(parts),
        ], capture_output=True, text=True)
        self.assertEqual(cut.returncode, 0, cut.stderr)
        joined = self.directory / "joined.wav"
        join = subprocess.run([
            sys.executable, "-B", str(SCRIPT.parent / "pinjie.py"),
            str(parts / "part1.wav"), str(parts / "part2.wav"), "-o", str(joined),
        ], capture_output=True, text=True)
        self.assertEqual(join.returncode, 0, join.stderr)
        with wave.open(str(original), "rb") as first, wave.open(str(joined), "rb") as second:
            self.assertEqual(first.getparams(), second.getparams())
            self.assertEqual(first.readframes(-1), second.readframes(-1))
        invalid = subprocess.run([
            sys.executable, "-B", str(SCRIPT.parent / "cut.py"), str(original), "9",
        ], capture_output=True, text=True)
        self.assertEqual(invalid.returncode, 1)


if __name__ == "__main__":
    unittest.main()
