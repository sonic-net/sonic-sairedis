[![Total alerts](https://img.shields.io/lgtm/alerts/g/Azure/sonic-sairedis.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/Azure/sonic-sairedis/alerts/)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/Azure/sonic-sairedis.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/Azure/sonic-sairedis/context:cpp)

VS[![Build Status](https://sonic-jenkins.westus2.cloudapp.azure.com/job/vs/job/sonic-sairedis-build/badge/icon)](https://sonic-jenkins.westus2.cloudapp.azure.com/job/vs/job/sonic-sairedis-build/)

# SONiC - SAI Redis - sairedis

## Description
The SAI Redis provides a SAI redis service that built on top of redis database. It contains two major components, 1) a SAI library
that puts SAI objects into the redis database, 2) a syncd that takes the SAI objects and puts them into the ASIC.

## Getting Started

### Install

Before installing, add key and package sources:

    sudo apt-key adv --keyserver apt-mo.trafficmanager.net --recv-keys 417A0893
    echo 'deb http://apt-mo.trafficmanager.net/repos/sonic/ trusty main' | sudo tee -a /etc/apt/sources.list.d/sonic.list
    sudo apt-get update

Install dependencies:

    sudo apt-get install redis-server -t trusty
    sudo apt-get install libhiredis0.13 -t trusty

Install building dependencies:

    sudo apt-get install libtool autoconf dh-exec

There are a few different ways you can install sairedis.

#### Install from Debian Repo

For your convenience, you can install prepared packages on Debian Jessie:

    sudo apt-get install libsairedis syncd

#### Install from Source

Checkout the source: `git clone https://github.com/Azure/sonic-sairedis.git` and install it yourself.

You will also need SAI submodule: `git submodule update --init --recursive`

Get SAI header files into /usr/include/sai. Put the SAI header files that you use to compile
libsairedis into /usr/include/sai

Get ASIC SDK and SAI packages from your ASIC vendor and install them.

Install prerequisite packages:

    sudo apt-get install libswsscommon libswsscommon-dev libhiredis-dev libzmq3-dev libpython-dev

Note: libswsscommon-dev requires libnl-3-200-dev, libnl-route-3-200-dev and libnl-nf-3-200-dev version >= 3.5.0. If these are not available via apt repositories, you can get them from the latest sonic-buildimage Jenkins build: https://sonic-jenkins.westus2.cloudapp.azure.com/job/vs/job/buildimage-vs-all/lastSuccessfulBuild/artifact/target/debs/buster/.

Install SAI dependencies:

    sudo apt-get install doxygen graphviz aspell

You can compile and install from source using:

    ./autogen.sh
    ./configure
    make && sudo make install

You can also build a debian package using:

    ./autogen.sh
    fakeroot debian/rules binary

If you do not have libsai, you can build a debian package using:

    ./autogen.sh
    fakeroot debian/rules binary-syncd-vs

## Need Help?

For general questions, setup help, or troubleshooting:
- [sonicproject on Google Groups](https://groups.google.com/d/forum/sonicproject)

For bug reports or feature requests, please open an Issue.

## Contribution guide

See the [contributors guide](https://github.com/Azure/SONiC/blob/gh-pages/CONTRIBUTING.md) for information about how to contribute.

### GitHub Workflow

We're following basic GitHub Flow. If you have no idea what we're talking about, check out [GitHub's official guide](https://guides.github.com/introduction/flow/). Note that merge is only performed by the repository maintainer.

Guide for performing commits:

* Isolate each commit to one component/bugfix/issue/feature
* Use a standard commit message format:

>     [component/folder touched]: Description intent of your changes
>
>     [List of changes]
>
> 	  Signed-off-by: Your Name your@email.com

For example:

>     swss-common: Stabilize the ConsumerTable
>
>     * Fixing autoreconf
>     * Fixing unit-tests by adding checkers and initialize the DB before start
>     * Adding the ability to select from multiple channels
>     * Health-Monitor - The idea of the patch is that if something went wrong with the notification channel,
>       we will have the option to know about it (Query the LLEN table length).
>
>       Signed-off-by: user@dev.null


* Each developer should fork this repository and [add the team as a Contributor](https://help.github.com/articles/adding-collaborators-to-a-personal-repository)
* Push your changes to your private fork and do "pull-request" to this repository
* Use a pull request to do code review
* Use issues to keep track of what is going on

