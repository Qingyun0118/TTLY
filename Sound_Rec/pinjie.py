import wave
import argparse
import sys


def concatenate_wavs(input_files, output_file):
    """
    将多个WAV文件拼接成一个文件

    :param input_files: 输入WAV文件路径列表
    :param output_file: 输出WAV文件路径
    """
    # 参数有效性检查
    if len(input_files) < 2:
        raise ValueError("至少需要两个输入文件进行拼接")

    # 读取第一个文件的参数
    with wave.open(input_files[0], 'rb') as first_file:
        params = first_file.getparams()
        frames = [first_file.readframes(-1)]

    # 检查并追加后续文件
    for file in input_files[1:]:
        with wave.open(file, 'rb') as next_file:
            # 验证参数一致性
            if (next_file.getnchannels() != params.nchannels or
                    next_file.getsampwidth() != params.sampwidth or
                    next_file.getframerate() != params.framerate):
                raise ValueError("音频参数不匹配：\n"
                                 f"文件1：声道数={params.nchannels}, 采样宽度={params.sampwidth}, 采样率={params.framerate}\n"
                                 f"文件{input_files.index(file) + 1}：声道数={next_file.getnchannels()}, "
                                 f"采样宽度={next_file.getsampwidth()}, 采样率={next_file.getframerate()}")
            frames.append(next_file.readframes(-1))

    # 写入合并后的文件
    with wave.open(output_file, 'wb') as output:
        output.setparams(params)
        output.writeframes(b''.join(frames))


def main(argv=None):
    parser = argparse.ArgumentParser(description="按输入顺序拼接参数一致的 WAV 文件")
    parser.add_argument("input_files", nargs="+", help="输入 WAV 文件，至少两个")
    parser.add_argument("-o", "--output", required=True, help="输出 WAV 文件路径")
    args = parser.parse_args(argv)
    try:
        concatenate_wavs(args.input_files, args.output)
        print(f"拼接完成: {args.output}")
        return 0
    except Exception as exc:
        print(f"拼接失败: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
