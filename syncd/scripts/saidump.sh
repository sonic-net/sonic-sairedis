#!/bin/bash
set -e

save_saidump_by_rdb()
{
  local filepath="/var/run/redis/sonic-db/database_config.json"

  local redis_socket redis_dir
  redis_socket=$(python3 -c '
import json
import sys

with open(sys.argv[1]) as config_file:
    config = json.load(config_file)
instance = config["DATABASES"]["ASIC_DB"]["instance"]
print(config["INSTANCES"][instance]["unix_socket_path"])
' "$filepath")
  redis_dir=$(dirname -- "$redis_socket")

  logger "saidump.sh: [1] Export the local ASIC_DB RDB snapshot."
  sonic-db-cli -s --authenticate --rdb "$redis_dir/dump.rdb" ASIC_DB > /dev/null 2>&1

  logger "saidump.sh: [2] Run rdb-cli command to convert the dump files into JSON files."
  rdb-cli "$redis_dir/dump.rdb" json | tee "$redis_dir/dump.json" > /dev/null

  logger "saidump.sh: [3] Run saidump -r to get the result at standard output from the JSON file."
  saidump -r "$redis_dir/dump.json" -m 100

  logger "saidump.sh: [4] Clear the temporary files."
  rm -f "$redis_dir/dump.rdb"
  rm -f "$redis_dir/dump.json"
}

save_saidump_by_rdb
