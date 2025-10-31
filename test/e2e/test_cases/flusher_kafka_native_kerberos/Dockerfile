# Copyright 2025 iLogtail Authors

FROM bash

COPY run.sh /run.sh
RUN chmod 755 /run.sh
CMD ["sh", "-c", "/run.sh >> /root/flusher_kerberos.log"]
