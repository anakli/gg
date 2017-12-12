# gg [![Build Status](https://travis-ci.org/StanfordSNR/gg.svg?branch=master)](https://travis-ci.org/StanfordSNR/gg)

![](https://s3-us-west-2.amazonaws.com/stanfordsnr/gg-xkcd.jpg)

## Build directions

To build `gg` you need the following packages:

- `gcc` >= 7.0
- `protobuf-compiler`, `libprotobuf-dev` >= 3.0
- `libcrypto++-dev` >= 5.6.3
- `python3`
- `libcap-dev`
- `libcajun-dev`
- `libkeyutils-dev`
- `libncurses5-dev`
- `libboost-dev`
- `libssl-dev`
- `autopoint`
- `help2man`
- `texinfo`
- `automake`
- `libtool`
- `pkg-config`

You can install this dependencies in Ubuntu (17.04 or newer) by running:

```
sudo apt-get install gcc-7 g++-7 protobuf-compiler libprotobuf-dev \
                     libcrypto++-dev libcap-dev libcajun-dev libkeyutils-dev \
                     libncurses5-dev libboost-dev libssl-dev autopoint help2man \
                     texinfo automake libtool pkg-config
```

To build `gg`, run the following commands:

```
./fetch-toolchain.sh
./autogen.sh
./configure
make -j$(nproc)
sudo make install
```

## Usage

### Environment Variables

To use `gg`, the following environment variables must be set:

- `GG_MODELPATH` => **absolute path** to `<gg-source-dir>/src/models/wrappers`.
- `GG_S3_BUCKET` => S3 bucket that is going to be used for remote execution (e.g. `gg-fun-bucket`).
- `GG_S3_REGION` => the region in which S3 bucket lives (e.g. `us-west-2`).
- `GG_LAMBDA_ROLE` => the role that will be assigned to the executed Lambda.
functions. Must have *AmazonS3FullAccess* and *AWSLambdaBasicExecutionRole*
permissions.
- `AWS_ACCESS_KEY_ID`, `AWS_SECRET_ACCESS_KEY` => your AWS access key


### Setting up Redis storage for gg

The default storage system for gg with AWS Lambda is [S3](https://aws.amazon.com/s3). This fork of gg provides a [Redis](https://redis.io) storage backend option for gg with AWS Lambda. To use the Redis backed, run gg-force with `GG_REDIS=1` (see example at the end of this README). 

To set up Redis, you can either download the Redis code and run it on a regular EC2 instance or use the AWS [ElastiCache](https://aws.amazon.com/elasticache/) service. When using the Redis backed for gg, AWS lambda workers are configured with access to the Virtual Private Cloud (VPC) where the Redis service is running. Therefore, you will need to setup a VPC, subnet, security group, internet gateway and route table. 

##### Install Redis-related dependencies:

~~~
git clone https://github.com/hmartiro/redox.git
sudo apt-get install -y cmake build-essential
sudo apt-get install -y libhiredis-dev libev-dev
cd redox
mkdir build && cd build
cmake ..
make
sudo make install
export LD_LIRABRY_PATH=$LD_LIBRARY_PATH:/usr/local/lib64

pip install redis
pip install --target /path/to/gg/src/remote redis
~~~

##### Set Redis-related envrionment variables:
To use `gg` with the Redis storage backend, the following environment variables must be set (in addition to the environment variables above):

- `GG_REDIS_HOSTADDR` => public IP address of Redis EC2 node or redis elasticache service.
- `GG_REDIS_PRIVATE_HOSTADDR` => private IP address of Redis EC2 node.
- `GG_VPC_SECURITY_GROUP_ID` => security group id of VPC where Redis is running.
- `GG_VPC_SUBNET_ID` => subnet id of VPC where Redis is running.


Recompile ggfunctions with `GG_REDIS=1` flag if using Redis:

~~
cd /path/to/gg/src/remote
GG_REDIS=1 make ggfunctions
~~


### Installing the gg Functions

After setting the environment variables, you need to install `gg` functions on
AWS Lambda. To do so:

~~~
cd src/remote/
make ggfunctions
~~~

### Example

To build [`mosh`](https://github.com/mobile-shell/mosh) using `gg`, first we
checkout the mosh source code:

~~~
git clone https://github.com/mobile-shell/mosh
~~~

Make sure that you have all the dependencies for building `mosh`. In Ubuntu,
you can run:

~~~
sudo apt-get install -y automake libtool g++ protobuf-compiler libprotobuf-dev \
                        libboost-dev libutempter-dev libncurses5-dev \
                        zlib1g-dev libio-pty-perl libssl-dev pkg-config
~~~

Inside the `mosh` directory, first you need to prepare `mosh` to build:

~~~
./autogen.sh
./configure
~~~

Then,

~~~
gg-init # create .gg directory
gg-infer make -j$(nproc) # creates the thunks
~~~

Now, to actually compile `mosh-server` on AWS Lambda with 100-way parallelism,
with the default S3 storage system, you can execute:

~~~
GG_LAMBDA=1 gg-force --jobs 100 --status src/frontend/mosh-server
~~~


To run the same job wih the Redis storage backend,
you can execute:

~~~
GG_LAMBDA=1 GG_REDIS=1 gg-force --jobs 100 --status src/frontend/mosh-server
~~~

To enable printing of infile and output file sizes, add `GG_SIZELOGS=1` to these commands.
