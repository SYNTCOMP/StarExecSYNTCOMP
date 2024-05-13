FROM centos7base

ADD MONA /home/MONA
RUN cd /home/MONA && autoreconf -vfi && ./configure && make && make install

COPY cudd-3.0.0.tar.gz /home
RUN cd /home/ && tar -xf cudd-3.0.0.tar.gz && cd cudd-3.0.0 && \
    ./configure --enable-silent-rules --enable-obj --enable-dddmp && make -j 8 && make install

COPY spot-2.11.6.dev.tar.gz /home
RUN cd /home/ && tar -xf spot-2.11.6.dev.tar.gz && cd spot-2.11.6.dev && \
    ./configure  --disable-devel && make -j 16 && make install

ADD lisa /home/lisa
RUN cd /home/lisa && rm -rf build && mkdir build && cd build && cmake .. && make -j 4 && make install

ADD ltlfcheck /home/ltlfcheck
RUN cd /home/ltlfcheck && rm -rf build && mkdir build && cd build && cmake .. && make -j 4 && make install

COPY syfco /usr/bin/syfco