# Running Tests 

## Overview

Validation scripts that are normally run when a pull request for sonic-sairedis is
created can be manually run from source by following these steps.

## Start with a clean build

If you don't already have a build, create one.

```
$ make target/sonic-vs.img.gz
```

## Running the tests with make check

To run these tests, do the following:

In sonic-buildimage/rules/config, enable KEEP_SLAVE_ON:

```
KEEP_SLAVE_ON = yes
```

Then, build (do not make clean, just start a new build). In the following example, 
primary build is stretch-based at the time of writing:

```
$ make target/sonic-vs.img.gz
```

If the build complains nothing to do, remove the target 

```
$ make target/sonic-vs.img.gz
make: 'target/sonic-vs.img.gz' is up to date.
$ rm target/sonic-vs.img.gz
```

and try again.

The first docker you will drop into is jessie, exit that docker by typing 
exit at the prompt, and the build will continue with stretch:

```
make: Nothing to be done for 'jessie'.
user@1c34bd0395d1:/sonic$ exit
exit
make[1]: Leaving directory '/home/slogan/sonic2/sonic-buildimage'
EXTRA_DOCKER_TARGETS=sonic-vs.img.gz BLDENV=stretch make -f Makefile.work stretch
make[1]: Entering directory '/home/slogan/sonic2/sonic-buildimage'
SONiC Build System
...
user@288feb4edd39:/sonic$
```

At this point, some debians from the build need to be installed. Copy and paste the following into 
a bash script, and execute to install redis, dependency libs, and enable rsyslog logging to aid
in debugging test script failures. Do this in the docker, in /sonic directory (paths in the 
script are relative to that location):

```
sudo dpkg -i target/debs/stretch/libhiredis*.deb
# Install libnl3
sudo dpkg -i target/debs/stretch/libnl-3-200_*.deb
sudo dpkg -i target/debs/stretch/libnl-3-dev_*.deb
sudo dpkg -i target/debs/stretch/libnl-genl-3-200_*.deb
sudo dpkg -i target/debs/stretch/libnl-genl-3-dev_*.deb
sudo dpkg -i target/debs/stretch/libnl-route-3-200_*.deb
sudo dpkg -i target/debs/stretch/libnl-route-3-dev_*.deb
sudo dpkg -i target/debs/stretch/libnl-nf-3-200_*.deb
sudo dpkg -i target/debs/stretch/libnl-nf-3-dev_*.deb
sudo dpkg -i target/debs/stretch/libnl-cli-3-200_*.deb
sudo dpkg -i target/debs/stretch/libnl-cli-3-dev_*.deb
# Install common library
sudo dpkg -i target/debs/stretch/libswsscommon_*.deb
sudo dpkg -i target/debs/stretch/libswsscommon-dev_*.deb
# Install REDIS
sudo apt-get install -y liblua5.1-0 lua-bitop lua-cjson
sudo dpkg -i target/debs/stretch/redis-tools_*.deb
sudo dpkg -i target/debs/stretch/redis-server_*.deb
sudo sed -ri 's/^# unixsocket/unixsocket/' /etc/redis/redis.conf
sudo sed -ri 's/^unixsocketperm .../unixsocketperm 777/' /etc/redis/redis.conf
sudo sed -ri 's/redis-server.sock/redis.sock/' /etc/redis/redis.conf
sudo service redis-server start
sudo service rsyslog start
```

Now you can execute the tests by issuing make check. Failures will be
reported to the console.


```
user@288feb4edd39:/sonic$ cd src/sonic-sairedis/tests
user@288feb4edd39:/sonic/src/sonic-sairedis/tests$ make check
...
```

Diagnosing failures can be aided by inspecting logs in /var/log/syslog
