import sys
import os
import subprocess
from threading import Thread
import yaml
import json
import time

Joblens_path = "/home/nowzycc/code/JobLens_cpp/build/JobLens"
Config_path = "/home/nowzycc/code/JobLens_cpp/config/config.yaml"
exec_path = "/usr/bin/sleep"
exec_args = ["10"]

def smoke_test():
    

    # 检查 Joblens 是否存在
    if not os.path.exists(Joblens_path):
        print(f"Joblens 路径不存在: {Joblens_path}")
        return False

    # 执行 Joblens
    try:
        exec_cmd = f"{Joblens_path} -c {Config_path} -e {exec_path} -a {' '.join(exec_args)}"
        print(f"执行命令: {exec_cmd}")

        result = subprocess.run(exec_cmd, shell=True, stdout=sys.stdout, stderr=sys.stderr, text=True)
        if result.returncode != 0:
            print(f"Joblens 执行失败: {result.stderr}")
            return False
        print("Joblens 执行成功")
        return True
    except Exception as e:
        print(f"执行 Joblens 时发生错误: {e}")
        return False

def add_job_test():
    correct_job_info = {
        'JobID':2,
        'JobPIDs':[1],
        'JobCreateTime':'2025-08-27 14:33:00',
    }
    err_job_info = {
        'JobID':3,
        'JobPIDs':[],
        'JobCreateTime':-1,
    }
    config = yaml.safe_load(open(Config_path))
    job_adder_path = config['collectors_config']['job_adder_fifo']
    def do_work():
        exec_cmd = f"{Joblens_path} -c {Config_path} -e {exec_path} -a {' '.join(exec_args)}"
        print(f"执行命令: {exec_cmd}")
        result = subprocess.run(exec_cmd, shell=True, stdout=sys.stdout, stderr=sys.stderr, text=True)
    t = Thread(target=do_work)
    t.start()
    time.sleep(2)  # 等待 Joblens 启动
    with open(job_adder_path, 'w') as f:
        f.write(json.dump(correct_job_info))
        f.flush()
        time.sleep(1)
        f.write(json.dump(err_job_info))
        f.flush()

if __name__ == "__main__":
    add_job_test()