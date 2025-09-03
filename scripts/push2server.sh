#!/bin/bash
exec_path=/build/JobLens-*-Linux.tar.gz
config_path=/config/config.yaml


exec_path=$(pwd)$exec_path
echo "exec_path: "$exec_path
echo "copying"
scp -i ~/.ssh/dog_contain $exec_path root@192.168.82.26:~/Joblens

config_path=$(pwd)$config_path
echo "config_path: "$config_path
echo "copying"
scp -i ~/.ssh/dog_contain $config_path root@192.168.82.26:~/Joblens/config/
