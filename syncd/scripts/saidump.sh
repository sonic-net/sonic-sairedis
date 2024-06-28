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
  print(data['INSTANCES']['redis']['hostname'], data['INSTANCES']['redis']['port'], data['INSTANCES']['redis']['unix_socket_path'])
  json_file.close()")

  # split
  redis_config=(${redis_config// / })
  local hostname=${redis_config[0]}
  local port=${redis_config[1]}
  local redis_dir=`dirname ${redis_config[2]}`
  logger "saidump.sh: hostname:$hostname, port:$port, redis_dir:$redis_dir"

  logger "saidump.sh: [1] Get the remote backups of RDB file."
  redis-cli -h $hostname -p $port --rdb $redis_dir/dump.rdb > /dev/null 2>&1

  logger "saidump.sh: [2] Run rdb-cli command to convert the dump files into JSON files."
  rdb-cli $redis_dir/dump.rdb json | tee $redis_dir/dump.json > /dev/null

  logger "saidump.sh: [3] Run saidump -r to update the JSON files' format as same as the saidump before. Then we can get the saidump's result in standard output."
  saidump -r $redis_dir/dump.json -m 100

  logger "saidump.sh: [4] Clear the temporary files."
  rm -f $redis_dir/dump.rdb
  rm -f $redis_dir/dump.json
}

save_saidump_by_rdb