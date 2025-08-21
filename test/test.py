
Joblens_path = "/home/nowzycc/code/JobLens_cpp/build/JobLens"
Config_path = "/home/nowzycc/code/JobLens_cpp/config/config.yaml"
exec_path = "/usr/bin/sleep"
exec_args = ["10"]

def smoke_test():
    import os
    import subprocess

    # 检查 Joblens 是否存在
    if not os.path.exists(Joblens_path):
        print(f"Joblens 路径不存在: {Joblens_path}")
        return False

    # 执行 Joblens
    try:
        exec_cmd = f"{Joblens_path} -c {Config_path} -e {exec_path} -a {' '.join(exec_args)}"
        print(f"执行命令: {exec_cmd}")
        result = subprocess.run(exec_cmd, shell=True, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"Joblens 执行失败: {result.stderr}")
            return False
        print("Joblens 执行成功")
        return True
    except Exception as e:
        print(f"执行 Joblens 时发生错误: {e}")
        return False
    
if __name__ == "__main__":
    smoke_test()