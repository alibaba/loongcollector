FROM sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/ilogtail-community-edition/ilogtail-build-linux:latest

ARG USERNAME=admin
USER root

# Create the user
COPY .env /tmp/.env
RUN source /tmp/.env && rm /tmp/.env; \
    if getent passwd $USERNAME; then userdel -f $USERNAME; fi; \
    if [ $HOST_OS = "Linux" ]; then \
    if getent group $GROUPNAME; then groupdel $GROUPNAME; fi; \
    groupadd --gid $GROUP_GID $GROUPNAME; \
    fi; \
    useradd --uid $USER_UID --gid $GROUP_GID -m $USERNAME; \
    echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME; \
    chmod 0440 /etc/sudoers.d/$USERNAME; \
    chown -R $USERNAME:$GROUPNAME /opt/logtail $(eval echo ~$USERNAME); \
    chmod -R 755 $(eval echo ~$USERNAME);

USER $USERNAME