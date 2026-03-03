from pathlib import Path
import tomllib
import threading
import queue
import time
import shutil
import subprocess
import os
import re

def clean_fullwidth_spaces(root_folder):
    pattern = re.compile(r"^　$", flags=re.MULTILINE)
    print(f"开始遍历文件夹: {root_folder} ...")

    # 全角空格的 Unicode 编码
    full_width_space = '\u3000'
    # 指定编码
    encoding_type = 'utf-16-le'

    # 递归遍历文件夹
    for dirpath, dirnames, filenames in os.walk(root_folder):
        for filename in filenames:

            if filename.lower().endswith('.ks'):
                file_path = os.path.join(dirpath, filename)
                
                try:
                    # 1. 读取文件
                    with open(file_path, 'r', encoding=encoding_type) as f:
                        lines = f.readlines()

                    original_count = len(lines)
                    
                    # 2. 过滤行
                    # line.rstrip('\r\n') 去除行尾换行符，确保只比较内容
                    # 如果内容仅仅等于全角空格，则剔除
                    new_lines = [line for line in lines if line.rstrip('\r\n') != full_width_space]

                    removed_count = original_count - len(new_lines)

                    if removed_count == 0:
                        print("未发现仅包含全角空格的行，文件未修改。")
                        return

                    # 3. 写入文件 (覆盖原文件)
                    with open(file_path, 'w', encoding=encoding_type) as f:
                        f.writelines(new_lines)

                    print(f"处理完成: 已删除 {removed_count} 行。")

                except UnicodeDecodeError:
                    print(f"[跳过] 编码错误: {file_path}")
                except Exception as e:
                    print(f"[出错] {file_path}: {e}")

    print("处理完成。")

try:
    basePath = Path(r"C:\Users\julixian\Desktop\Works\VS\JLXHP\Release\DaunyaSanToKainushiKun\DaunyaSanToKainushiKun_cn_base")
    newCharMapPath = basePath / "base"
    newScPath = basePath / "../DaunyaSanToKainushiKun_cn_script/project_cn"

    clean_fullwidth_spaces("textproc/new")
    shutil.copytree("textproc/new", newScPath, dirs_exist_ok=True)
    result = subprocess.run(
                ["MergeJsonTransMap.exe", "textproc\\trans", "textproc\\orig", newCharMapPath / "transMap.json"],
                capture_output=True,  # 捕获输出
                text=True,            # 以文本形式返回
                encoding='utf-8'
            )
    print(f"返回输出: {result.stdout}")
    input("Done.")

except Exception as e:
    print(f"Error during unload(): {e}")
    