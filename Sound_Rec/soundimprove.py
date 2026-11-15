"""YAMNet 特征与 XGBoost 海洋动物声音分类。"""

import argparse
import os
from pathlib import Path
import sys


SCRIPT_DIR = Path(__file__).resolve().parent
DATA_DIR = SCRIPT_DIR.parent / "train"
MODEL_SAVE_PATH = SCRIPT_DIR / "animal_classifier_pro.joblib3"
YAMNET_URL = "https://tfhub.dev/google/yamnet/1"
SAMPLE_RATE = 16000
CLASS_NAMES = [
    "Bearded Seal", "Beluga White Whale", "Harp Seal", "Humpback Whale",
    "Killer Whale", "Long-Finned Pilot Whale", "Northern Right Whale",
    "Pantropical Spotted Dolphin", "Ross Seal", "Sperm Whale", "Walrus",
    "White-beaked Dolphin",
]


def load_dependencies():
    """在真正识别或训练时加载依赖，使 --help 可独立使用。"""
    global np, librosa, tf, hub, XGBClassifier, LabelEncoder, StandardScaler, dump, load
    import numpy as np
    import librosa
    import tensorflow as tf
    import tensorflow_hub as hub
    from xgboost import XGBClassifier
    from sklearn.preprocessing import LabelEncoder, StandardScaler
    from joblib import dump, load


class AudioSegmenter:
    """使用约 0.975 秒窗口和 0.3125 秒帧移处理长音频。"""

    def __init__(self, frame_length=15600, hop_length=5000):
        self.frame_length = frame_length
        self.hop_length = hop_length

    def segment(self, audio):
        if len(audio) <= self.frame_length:
            return [np.pad(audio, (0, self.frame_length - len(audio)))]
        return [
            audio[start:start + self.frame_length]
            for start in range(0, len(audio) - self.frame_length + 1, self.hop_length)
        ]


class MarineAnimalClassifier:
    def __init__(self, yamnet=YAMNET_URL):
        load_dependencies()
        self.model = None
        self.scaler = StandardScaler()
        self.label_encoder = LabelEncoder()
        self.segmenter = AudioSegmenter()
        self.yamnet_model = hub.load(yamnet)

    def _load_audio(self, file_path):
        audio, _ = librosa.load(file_path, sr=SAMPLE_RATE, mono=True)
        if len(audio) == 0:
            raise ValueError(f"空音频文件: {file_path}")
        return audio.flatten()

    def _extract_features(self, audio):
        audio = audio[:15600] if len(audio) > 15600 else np.pad(audio, (0, 15600 - len(audio)))
        waveform = tf.reshape(tf.convert_to_tensor(audio, dtype=tf.float32), [-1])
        _, embeddings, _ = self.yamnet_model(waveform)
        return np.concatenate([np.mean(embeddings, axis=0), np.std(embeddings, axis=0)])

    def train(self, data_dir=DATA_DIR, model_path=MODEL_SAVE_PATH):
        features, labels = [], []
        for class_idx, class_name in enumerate(CLASS_NAMES):
            class_dir = Path(data_dir) / class_name
            if not class_dir.is_dir():
                continue
            print(f"处理类别: {class_name}", flush=True)
            for file_path in sorted(class_dir.iterdir()):
                if not file_path.is_file() or file_path.suffix.lower() != ".wav":
                    continue
                audio = self._load_audio(file_path)
                for frame in self.segmenter.segment(audio):
                    features.append(self._extract_features(frame))
                    labels.append(class_idx)
        if len(set(labels)) < 2:
            raise ValueError("训练至少需要两个类别的有效 WAV 音频")

        X = self.scaler.fit_transform(features)
        y = self.label_encoder.fit_transform(labels)
        self.model = XGBClassifier(
            n_estimators=700,
            learning_rate=0.05,
            max_depth=9,
            subsample=0.8,
            eval_metric="mlogloss",
        )
        self.model.fit(X, y)
        model_path = Path(model_path)
        model_path.parent.mkdir(parents=True, exist_ok=True)
        dump({
            "model": self.model,
            "label_encoder": self.label_encoder,
            "scaler": self.scaler,
        }, model_path)
        print(f"模型保存至 {model_path}", flush=True)

    def load_model(self, model_path):
        saved = load(model_path)
        self.model = saved["model"]
        self.label_encoder = saved["label_encoder"]
        self.scaler = saved["scaler"]

    def predict_long_audio(self, audio_path):
        audio = self._load_audio(audio_path)
        frames = self.segmenter.segment(audio)
        features = np.mean([self._extract_features(frame) for frame in frames], axis=0)
        probas = self.model.predict_proba(self.scaler.transform([features]))[0]
        top2 = np.argsort(probas)[-2:][::-1]
        class_ids = self.label_encoder.inverse_transform(self.model.classes_[top2])
        return [(CLASS_NAMES[int(class_id)], float(probas[index]))
                for class_id, index in zip(class_ids, top2)]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--audio", action="append", type=Path, default=[],
                        help="待识别音频，可重复指定以批量处理")
    parser.add_argument("--model", type=Path, default=MODEL_SAVE_PATH,
                        help="分类模型路径，默认脚本旁 animal_classifier_pro.joblib3")
    parser.add_argument("--train", action="store_true", help="显式训练并保存模型；默认只识别")
    parser.add_argument("--data-dir", type=Path, default=DATA_DIR,
                        help="训练数据目录，默认项目根目录 train/，按类别名称分文件夹")
    parser.add_argument("--yamnet", default=os.environ.get("YAMNET_MODEL") or YAMNET_URL,
                        help="YAMNet SavedModel 目录或 TF Hub URL，亦可设置 YAMNET_MODEL")
    parser.add_argument("--cache-dir", type=Path,
                        default=os.environ.get("TFHUB_CACHE_DIR") or SCRIPT_DIR / ".cache" / "tfhub",
                        help="TF Hub 下载缓存，亦可设置 TFHUB_CACHE_DIR")
    args = parser.parse_args(argv)
    if not args.train and not args.audio:
        parser.error("请指定 --audio，或使用 --train 训练模型")

    model_path = args.model.expanduser().resolve()
    data_dir = args.data_dir.expanduser().resolve()
    if args.train and not data_dir.is_dir():
        print(f"训练数据目录不存在: {data_dir}", file=sys.stderr)
        return 1
    if not args.train and not model_path.is_file():
        print(f"分类模型不存在: {model_path}", file=sys.stderr)
        return 1

    failed = False
    audio_paths = []
    for path in args.audio:
        path = path.expanduser().resolve()
        if path.is_file():
            audio_paths.append(path)
        else:
            print(f"音频文件不存在: {path}", file=sys.stderr)
            failed = True
    if not args.train and not audio_paths:
        return 1

    yamnet = args.yamnet
    if not yamnet.startswith(("http://", "https://")):
        yamnet_path = Path(yamnet).expanduser().resolve()
        if not yamnet_path.is_dir():
            print(f"YAMNet SavedModel 目录不存在: {yamnet_path}", file=sys.stderr)
            return 1
        yamnet = str(yamnet_path)

    try:
        cache_dir = args.cache_dir.expanduser().resolve()
        cache_dir.mkdir(parents=True, exist_ok=True)
        os.environ["TFHUB_CACHE_DIR"] = str(cache_dir)
        classifier = MarineAnimalClassifier(yamnet=yamnet)
        if args.train:
            classifier.train(data_dir=data_dir, model_path=model_path)
        else:
            classifier.load_model(model_path)
    except ImportError as exc:
        print(f"缺少或无法加载识别依赖: {exc}；请安装 Sound_Rec/requirements.txt", file=sys.stderr)
        return 1
    except Exception as exc:
        print(f"初始化或训练失败: {exc}", file=sys.stderr)
        return 1

    for audio_path in audio_paths:
        print(f"处理文件: {audio_path}", flush=True)
        try:
            results = classifier.predict_long_audio(audio_path)
            if not results:
                raise ValueError("模型未返回识别结果")
            for name, probability in results:
                print(f"- {name} ({probability:.2f})", flush=True)
        except Exception as exc:
            print(f"识别失败 {audio_path}: {exc}", file=sys.stderr)
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
