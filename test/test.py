import sys
import os
import subprocess
from threading import Thread
import yaml
import json
import time
from flask import Flask, request, jsonify,app

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
        f.write(json.dumps(correct_job_info))
        f.flush()
        print("Wrote correct job info")
        time.sleep(1)
        f.write(json.dumps(err_job_info))
        f.flush()
        print("Wrote erroneous job info")

Config_path = os.getenv("CONFIG_PATH", "config.yaml")   # 可按需修改

def add_job(JobID, JobPIDs, CreateTime):
    job_info = {
        'JobID': JobID,
        'JobPIDs': JobPIDs,
        'JobCreateTime': CreateTime,
    }
    with open(Config_path) as cfg_file:
        config = yaml.safe_load(cfg_file)

    job_adder_path = config['collectors_config']['job_adder_fifo']
    with open(job_adder_path, 'w') as f:
        f.write(json.dumps(job_info))
        f.flush()

@app.route('/add_job', methods=['POST'])
def api_add_job():
    """
    POST /add_job
    Body(json):
    {
        "JobID": "abc123",
        "JobPIDs": [1234, 5678],
        "CreateTime": "2025-08-27 14:00:00"
    }
    """
    data = request.get_json(force=True, silent=True) or {}
    JobID = data.get('JobID')
    JobPIDs = data.get('JobPIDs')
    CreateTime = data.get('CreateTime')

    # 简单校验
    if JobID is None or JobPIDs is None or CreateTime is None:
        return jsonify({'error': 'JobID, JobPIDs and CreateTime are required'}), 400

    try:
        add_job(JobID, JobPIDs, CreateTime)
    except Exception as e:
        # 记录日志可换成 logging
        return jsonify({'error': str(e)}), 500

    return jsonify({'status': 'ok'}), 200

if __name__ == "__main__":
    add_job_test()