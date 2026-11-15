import wave
import argparse
from pathlib import Path
import sys


def split_wav(input_file, split_seconds_list, output_dir="."):
    with wave.open(input_file, 'rb') as wav_in:
        params = wav_in.getparams()
        total_frames = params.nframes
        framerate = params.framerate
        total_duration = total_frames / framerate

        # 转换并验证分割点
        try:
            split_frames = sorted([int(s * framerate) for s in split_seconds_list])
        except TypeError:
            raise ValueError("分割时间必须为数字")

        # 验证分割点有效性
        if not split_frames:
            raise ValueError("至少需要提供一个分割点")
        if split_frames[0] <= 0 or split_frames[-1] >= total_frames:
            raise ValueError(f"分割点需在0到{total_duration:.2f}秒之间")
        for i in range(1, len(split_frames)):
            if split_frames[i] <= split_frames[i - 1]:
                raise ValueError("分割点必须递增且不重复")

        # 构建分割区间（添加首尾）
        split_points = [0] + split_frames + [total_frames]

        # 读取全部音频数据
        wav_in.rewind()
        all_frames = wav_in.readframes(total_frames)
        frame_size = params.sampwidth * params.nchannels  # 每帧字节数

        # 分割写入指定目录
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)
        for idx in range(1, len(split_points)):
            start = split_points[idx - 1]
            end = split_points[idx]
            start_byte = start * frame_size
            end_byte = end * frame_size
            part_frames = all_frames[start_byte:end_byte]

            with wave.open(str(output_dir / f'part{idx}.wav'), 'wb') as wav_out:
                wav_out.setparams(params)
                wav_out.writeframes(part_frames)
        return len(split_points) - 1  # 返回分割数量


def main(argv=None):
    parser = argparse.ArgumentParser(description="通用WAV文件分割工具")
    parser.add_argument("input_file", help="输入WAV文件路径")
    parser.add_argument("split_seconds", nargs='+', type=float,
                        help="分割时间列表（秒），例如：10 25 30.5")
    parser.add_argument("--output-dir", default=".", help="分割文件输出目录，默认当前目录")
    args = parser.parse_args(argv)

    try:
        num_parts = split_wav(args.input_file, args.split_seconds, args.output_dir)
        print(f"分割成功：生成 {num_parts} 个文件 (part1.wav ~ part{num_parts}.wav)")
        return 0
    except Exception as e:
        print(f"错误：{e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
