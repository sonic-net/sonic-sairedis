#!/bin/bash
#
# Configure the system redis-server for sonic-sairedis' tests.
# Applied via the `configure-redis-for-tests` post_install entry in
# build-env/packages/base.yaml.
#
# Reproduces exactly the redis config build-template.yml applied inline.
# NOTE (F11): sonic-sairedis does NOT set `notify-keyspace-events AKE`
# (unlike sonic-swss-common / sonic-swss) -- do not add it here.
set -ex

sudo sed -ri 's/^# unixsocket/unixsocket/'              /etc/redis/redis.conf
sudo sed -ri 's/^unixsocketperm .../unixsocketperm 777/' /etc/redis/redis.conf
sudo sed -ri 's/redis-server.sock/redis.sock/'          /etc/redis/redis.conf
sudo service redis-server start
