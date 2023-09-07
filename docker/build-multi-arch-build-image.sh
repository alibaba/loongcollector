#docker buildx build --platform linux/amd64,linux/arm64 \
#        -t sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/centos7-cve-fix:1.0.0 \
#        -o type=registry \
#        --no-cache -f Dockerfile.centos7-cve-fix .

#docker buildx build --platform linux/amd64,linux/arm64 \
#        -t sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail-toolchain-linux:1.2.0 \
#        -o type=registry \
#        --no-cache -f Dockerfile.ilogtail-toolchain-linux .

#docker buildx build --sbom=false --provenance=false --platform linux/amd64,linux/arm64 \
#        -t sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail-build-linux:1.2.0 \
#        -o type=registry \
#        --no-cache -f Dockerfile.ilogtail-build-linux .

#curl -L https://github.com/regclient/regclient/releases/latest/download/regctl-linux-amd64 >regctl

regctl image copy sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail-build-linux:1.2.0 \
        sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail-build-linux:gcc_9.3.1-1
