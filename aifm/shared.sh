#!/bin/bash

AIFM_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
SHENANGO_PATH=$AIFM_PATH/../shenango

SHENANGO_IPS=("18.18.1.3" "18.18.1.4" "18.18.1.5")
MEM_SERVER_PORT=8000
MEM_SERVER_STACK_KB=65536

source $AIFM_PATH/configs/ssh

function say_failed() {
    echo -e "----\e[31mFailed\e[0m"
}

function say_passed() {
    echo -e "----\e[32mPassed\e[0m"
}

function assert_success {
    if [[ $? -ne 0 ]]; then
        say_failed
        exit -1
    fi
}

# Executes command on ALL nodes in the cluster
function cluster_execute {
    local cmd=$1
    for i in "${!NODE_IPS[@]}"; do
        echo "Executing on Node $i..."
        ssh_execute $i "$cmd"
    done
}

function kill_process {
    pid=`pgrep $1`
    if [ -n "$pid" ]; then
    { sudo kill $pid && sudo wait $pid; } 2>/dev/null
    fi
}

function ssh_kill_process {
    local index=$1
    local proc_name=$2
    pid=`ssh_execute $index "pgrep $proc_name"`
    if [ -n "$pid" ]; then
    ssh_execute $index "{ sudo kill $pid && sudo wait $pid; } 2>/dev/null"
    fi
}

function kill_local_iokerneld {
    kill_process iokerneld
}

function run_local_iokerneld {
    kill_local_iokerneld
    sudo $SHENANGO_PATH/iokerneld $@ > /dev/null 2>&1 &
    disown -r
    assert_success
    sleep 3
}

function rerun_local_iokerneld {
    kill_local_iokerneld
    run_local_iokerneld simple
}

function rerun_local_iokerneld_noht {
    kill_local_iokerneld
    run_local_iokerneld simple noht
}

function rerun_local_iokerneld_args {
    kill_local_iokerneld
    run_local_iokerneld $@
}

# Note: Added index parameter to kill_mem_server to allow targeted kills
function kill_mem_server {
    for i in "${!NODE_IPS[@]}"; do
        ssh_kill_process $i iokerneld
        ssh_kill_process $i tcp_device_serv
    done
}

function run_mem_server {
    for i in "${!NODE_IPS[@]}"; do
        ssh_execute $i "sudo $SHENANGO_PATH/iokerneld simple" > /dev/null 2>&1 &
        sleep 2
        ssh_execute_tty $i "sudo sh -c 'ulimit -s $MEM_SERVER_STACK_KB; \
                         $AIFM_PATH/bin/tcp_device_server $AIFM_PATH/configs/server.config \
                         $MEM_SERVER_PORT'" > /dev/null 2>&1 &
    done
}

function rerun_mem_server {
    kill_mem_server
    run_mem_server
}

# Client side: Generate the comma-separated IP string for your C++ test
function get_cluster_conn_str {
    local conn_str=""
    for ip in "${SHENANGO_IPS[@]}"; do
        conn_str+="$ip:$MEM_SERVER_PORT,"
    done
    echo ${conn_str%,}
}

function run_program {    
    local conn_str=$(get_cluster_conn_str)
    sudo stdbuf -o0 sh -c "$1 $AIFM_PATH/configs/client.config $conn_str"
}