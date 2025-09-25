# Copyright 2025 iLogtail Authors

FROM ubuntu:20.04

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
      bash \
    && rm -rf /var/lib/apt/lists/*

COPY run.sh /run.sh
RUN chmod 755 /run.sh
CMD ["sh", "-c", "/run.sh >> /root/flusher_kerberos.log"]
