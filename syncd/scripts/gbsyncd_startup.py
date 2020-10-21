#!/usr/bin/python

'''
Copyright 2019 Broadcom. The term "Broadcom" refers to Broadcom Inc.
and/or its subsidiaries.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
  '''
import os
import sys
import json
import subprocess


def physyncd_enable(gearbox_config):
    i = 1
    for phy in gearbox_config['phys']:
        subprocess.Popen(["/bin/bash", "-c",  "/bin/bash -c {}".format('"exec /usr/bin/syncd -p /etc/sai.d/pai.profile -x /usr/share/sonic/hwsku/context_config.json -g {}"'.format(i))], close_fds=True)
        i += 1

def main():

    # Only privileged users can execute this command
    if os.geteuid() != 0:
        sys.exit("Root privileges required for this operation")

    """ Loads json file """
    try:
        with open('/usr/share/sonic/hwsku/gearbox_config.json') as file_object:
            gearbox_config=json.load(file_object)
    except:
        sys.exit("No external PHY / gearbox supported on this platform, existing physycd application")

    physyncd_enable(gearbox_config)


if __name__== "__main__":
    main()

