#!/bin/bash
set -e

save_saidump_by_rdb()
{
  local filepath="/var/run/redis/sonic-db/database_config.json"

  # Get hostname, port, redis directory
  local redis_settings
  redis_settings=$(python3 -c "
import json
with open('$filepath') as json_file:
  data = json.load(json_file)
  instance = data['INSTANCES']['redis']
  print(instance['hostname'], instance['port'], instance['unix_socket_path'], sep='\n')") || return 1

  local -a redis_config
  mapfile -t redis_config <<< "$redis_settings"
  local hostname=${redis_config[0]}
  local port=${redis_config[1]}
  local redis_dir
  redis_dir=$(dirname -- "${redis_config[2]}")
  logger "saidump.sh: hostname:$hostname, port:$port, redis_dir:$redis_dir"

  logger "saidump.sh: [1] Get the remote backups of RDB file."
  local -a rdb_args=()
  if [ "$port" -eq 0 ]; then
    rdb_args=(-s --authenticate)
  fi
  sonic-db-cli "${rdb_args[@]}" --rdb "$redis_dir/dump.rdb" ASIC_DB > /dev/null 2>&1 || return 1

  logger "saidump.sh: [2] Run rdb-cli command to convert the dump files into JSON files."
  rdb-cli "$redis_dir/dump.rdb" json | tee "$redis_dir/dump.json" > /dev/null

  logger "saidump.sh: [3] Run saidump -r to get the result at standard output from the JSON file."
  saidump -r "$redis_dir/dump.json" -m 100

  logger "saidump.sh: [4] Clear the temporary files."
  rm -f "$redis_dir/dump.rdb"
  rm -f "$redis_dir/dump.json"
}

save_saidump_by_rdb
