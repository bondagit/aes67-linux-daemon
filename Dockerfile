FROM ubuntu
RUN echo 'APT::Install-Suggests "0";' >> /etc/apt/apt.conf.d/00-docker
RUN echo 'APT::Install-Recommends "0";' >> /etc/apt/apt.conf.d/00-docker
RUN DEBIAN_FRONTEND=noninteractive \
  apt-get update && \
  apt-get install -qq -f -y build-essential clang git cmake libboost-all-dev valgrind linux-sound-base alsa-base alsa-utils libasound2-dev libavahi-client-dev \
  && rm -rf /var/lib/apt/lists/*
COPY . .
RUN ./buildfake.sh
WORKDIR ./daemon/tests
RUN ./daemon-test -p
