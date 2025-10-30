FROM aliyun/loongcollector:0.0.1
SHELL ["/bin/sh","-c"]
# Install Kerberos/GSSAPI runtime dependencies for librdkafka SASL_GSSAPI
RUN ( command -v yum >/dev/null 2>&1 && yum -y install -q cyrus-sasl-gssapi krb5-workstation ) || \
    ( command -v microdnf >/dev/null 2>&1 && microdnf -y install cyrus-sasl-gssapi krb5-workstation ) || \
    ( command -v apt-get >/dev/null 2>&1 && export DEBIAN_FRONTEND=noninteractive && apt-get update -qq && apt-get install -y -q libsasl2-modules libsasl2-modules-gssapi-mit krb5-user ) || true

# Ensure Cyrus SASL can find mechanism modules across common distro paths
ENV SASL_PATH=/usr/lib/x86_64-linux-gnu/sasl2:/usr/lib/sasl2
# Ensure krb5 picks the provided config and shared ccache by default
ENV KRB5_CONFIG=/etc/krb5.conf \
    KRB5CCNAME=FILE:/var/kerberos/krb5cc
