FROM centos:7

RUN yum install -y centos-release-scl && \
    yum-config-manager -y --enable rhel-server-rhscl-7-rpms && \
    yum install -y devtoolset-7 devtoolset-8 python3-devel

RUN yum install -y wget git flex bison automake autotools-latest libtool

RUN echo "source scl_source enable devtoolset-8" >> /root/.bashrc
SHELL [ "/usr/bin/scl", "enable", "devtoolset-8" ]

COPY CMake-3.14.7.tar.gz /home/
RUN cd /home/ && tar -xf CMake-3.14.7.tar.gz  && cd CMake-3.14.7 && \
    ./bootstrap && make -j 12 && make install
