#!/bin/bash
set -e

save_saidump_by_rdb()
{
  local filepath="/var/run/redis/sonic-db/database_config.json"

  # Get hostname, port, redis directory
  local redis_config=$(python3 -c "
import json
with open('$filepath') as json_file:
  data = json.load(json_file)
  print(data['INSTANCES']['redis']['hostname'], data['INSTANCES']['redis']['port'], data['INSTANCES']['redis']['unix_socket_path'])")

  # split
  redis_config=(${redis_config// / })
  local hostname=${redis_config[0]}
  local port=${redis_config[1]}
  local redis_dir=`dirname ${redis_config[2]}`
  logger "saidump.sh: hostname:$hostname, port:$port, redis_dir:$redis_dir"

  logger "saidump.sh: [1] Config Redis consistency directory."
  redis-cli -h $hostname -p $port CONFIG SET dir $redis_dir > /dev/null

  logger "saidump.sh: [2] SAVE."
  redis-cli -h $hostname -p $port SAVE > /dev/null

  logger "saidump.sh: [3] Run rdb command to convert the dump files into JSON files."
  rdb --command json  $redis_dir/dump.rdb | tee $redis_dir/dump.json > /dev/null

  logger "saidump.sh: [4] Run saidump -r to update the JSON files' format as same as the saidump before. Then we can get the saidump's result in standard output."
  saidump -r $redis_dir/dump.json -m 100

  logger "saidump.sh: [5] Clear the temporary files."
  rm -f $redis_dir/dump.rdb
  rm -f $redis_dir/dump.json
}

save_saidump_by_rdb