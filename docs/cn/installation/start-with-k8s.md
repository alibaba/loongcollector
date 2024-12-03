# Kubernetes使用

## 使用前提

已部署Kubernetes集群或[minikube](https://minikube.sigs.k8s.io/docs/start/)

具备访问Kubernetes集群的[kubectl](https://kubernetes.io/docs/tasks/tools/install-kubectl-linux/)

## 采集Kubernetes容器日志

1. 创建部署loongcollector的命名空间

    将下面内容保存为loongcollector-ns.yaml

    ```yaml {.line-numbers}
    apiVersion: v1
    kind: Namespace
    metadata:
      name: loongcollector
    ```

    您也可以直接从下面的地址下载示例配置。

    ```bash
    wget https://raw.githubusercontent.com/alibaba/ilogtail/main/example_config/start_with_k8s/loongcollector-ns.yaml
    ```

    应用上述配置

    ```bash
    kubectl apply -f loongcollector-ns.yaml
    ```

2. 创建配置loongcollector的ConfigMap和Secret

    将下面内容保存为loongcollector-user-configmap.yaml。该ConfigMap后续将作为配置目录挂载到loongcollector容器中，因此可包含多个采集配置。

    ```yaml {.line-numbers}
    apiVersion: v1
    kind: ConfigMap
    metadata:
      name: loongcollector-user-cm
      namespace: loongcollector
    data:
      nginx_stdout.yaml: |
        enable: true
        inputs:
          - Type: service_docker_stdout
            Stderr: false
            Stdout: true                # only collect stdout
            IncludeK8sLabel:
              app: nginx                # choose containers with this label
        processors:
          - Type: processor_regex       # structure log
            SourceKey: content
            Regex: '([\d\.:]+) - (\S+) \[(\S+) \S+\] \"(\S+) (\S+) ([^\\"]+)\" (\d+) (\d+) \"([^\\"]*)\" \"([^\\"]*)\" \"([^\\"]*)\"'
            Keys:
              - remote_addr
              - remote_user
              - time_local
              - method
              - url
              - protocol
              - status
              - body_bytes_sent
              - http_referer
              - http_user_agent
              - http_x_forwarded_for
        flushers:
          - Type: flusher_stdout
            OnlyStdout: true
    ```

    将下面内容保存为loongcollector-secret.yaml。该Secret为可选，当需要将日志写入SLS时会用到。

    ```yaml {.line-numbers}
    apiVersion: v1
    kind: Secret
    metadata:
      name: loongcollector-secret
      namespace: loongcollector
    type: Opaque
    data:
      access_key_id:  # base64 accesskey id if you want to flush to SLS
      access_key:     # base64 accesskey secret if you want to flush to SLS
    ```

    您也可以直接从下面的地址下载示例配置。

    ```bash
    wget https://raw.githubusercontent.com/alibaba/ilogtail/main/example_config/start_with_k8s/loongcollector-user-configmap.yaml
    wget https://raw.githubusercontent.com/alibaba/ilogtail/main/example_config/start_with_k8s/loongcollector-secret.yaml
    ```

    应用上述配置

    ```bash
    kubectl apply -f loongcollector-user-configmap.yaml
    kubectl apply -f loongcollector-secret.yaml
    ```

3. 创建loongcollector DaemonSet

    将下面内容保存为loongcollector-daemonset.yaml。

    ```yaml {.line-numbers}
    apiVersion: apps/v1
    kind: DaemonSet
    metadata:
      name: loongcollector-ds
      namespace: loongcollector
      labels:
        k8s-app: loongcollector-ds
    spec:
      selector:
        matchLabels:
          k8s-app: loongcollector-ds
      template:
        metadata:
          labels:
            k8s-app: loongcollector-ds
        spec:
          tolerations:
            - operator: Exists                    # deploy on all nodes
          containers:
            - name: loongcollector
              env:
                - name: ALIYUN_LOG_ENV_TAGS       # add log tags from env
                  value: _node_name_|_node_ip_
                - name: _node_name_
                  valueFrom:
                    fieldRef:
                      apiVersion: v1
                      fieldPath: spec.nodeName
                - name: _node_ip_
                  valueFrom:
                    fieldRef:
                      apiVersion: v1
                      fieldPath: status.hostIP
                - name: cpu_usage_limit           # loongcollector's self monitor cpu limit
                  value: "1"
                - name: mem_usage_limit           # loongcollector's self monitor mem limit
                  value: "512"
                - name: default_access_key_id     # accesskey id if you want to flush to SLS
                  valueFrom:
                    secretKeyRef:
                      name: loongcollector-secret
                      key: access_key_id
                      optional: true
                - name: default_access_key        # accesskey secret if you want to flush to SLS
                  valueFrom:
                    secretKeyRef:
                      name: loongcollector-secret
                      key: access_key
                      optional: true
              image: >-
                sls-opensource-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-community-edition/loongcollector:latest
              imagePullPolicy: IfNotPresent
              resources:
                limits:
                  cpu: 1000m
                  memory: 1Gi
                requests:
                  cpu: 400m
                  memory: 384Mi
              volumeMounts:
                - mountPath: /var/run                       # for container runtime socket
                  name: run
                - mountPath: /logtail_host                  # for log access on the node
                  mountPropagation: HostToContainer
                  name: root
                  readOnly: true
                - mountPath: /usr/local/loongcollector/data # for checkpoint between container restart
                  name: checkpoint
                - mountPath: /usr/local/loongcollector/conf/continuous_pipeline_config/local # mount config dir
                  name: user-config
                  readOnly: true
          dnsPolicy: ClusterFirstWithHostNet
          hostNetwork: true
          volumes:
            - hostPath:
                path: /var/run
                type: Directory
              name: run
            - hostPath:
                path: /
                type: Directory
              name: root
            - hostPath:
                path: /etc/loongcollector-loongcollector-ds/checkpoint
                type: DirectoryOrCreate
              name: checkpoint
            - configMap:
                defaultMode: 420
                name: loongcollector-user-cm
              name: user-config
    ```

    您也可以直接从下面的地址下载示例配置。

    ```bash
    wget https://raw.githubusercontent.com/alibaba/ilogtail/main/example_config/start_with_k8s/loongcollector-daemonset.yaml
    ```

    添加参数示例

    ```yaml
            - name: loongcollector
              command:
              - /usr/local/loongcollector/loongcollector_control.sh
              args:
              - "start_and_block"
              - "-enable_containerd_upper_dir_detect=true"
              - "-dirfile_check_interval_ms=5000"
              - "-logtail_checkpoint_check_gc_interval_sec=120"
    ```

    应用上述配置

    ```bash
    kubectl apply -f loongcollector-daemonset.yaml
    ```

4. 部署用来测试的nginx

    将下面内容保存为nginx-deployment.yaml。

    ```yaml {.line-numbers}
    apiVersion: apps/v1
    kind: Deployment
    metadata:
      name: nginx
      namespace: default
      labels:
        app: nginx
    spec:
      replicas: 1
      selector:
        matchLabels:
          app: nginx
      template:
        metadata:
          labels:
            app: nginx
        spec:
          containers:
            - image: 'nginx:latest'
              name: nginx
              ports:
                - containerPort: 80
                  name: http
                  protocol: TCP
              resources:
                requests:
                  cpu: 100m
                  memory: 100Mi
    ```

    您也可以直接从下面的地址下载示例配置。

    ```bash
    wget https://raw.githubusercontent.com/alibaba/ilogtail/main/example_config/start_with_k8s/nginx-deployment.yaml
    ```

    应用上述配置

    ```bash
    kubectl apply -f nginx-deployment.yaml
    ```

5. 发送请求构造示例日志

    ```bash
    kubectl exec nginx-<pod-id> -- curl localhost/hello/loongcollector
    ```

6. 查看采集到的测试容器标准输出日志

    ```bash
    kubectl logs loongcollector-ds-<pod-id> -n loongcollector
    ```

    结果为

    ```json
    2022-07-14 16:36:50 {"_time_":"2022-07-15T00:36:48.489153485+08:00","_source_":"stdout","_image_name_":"docker.io/library/nginx:latest","_container_name_":"nginx","_pod_name_":"nginx-76d49876c7-r892w","_namespace_":"default","_pod_uid_":"07f75a79-da69-40ac-ae2b-77a632929cc6","_container_ip_":"10.223.0.154","remote_addr":"::1","remote_user":"-","time_local":"14/Jul/2022:16:36:48","method":"GET","url":"/hello/loongcollector","protocol":"HTTP/1.1","status":"404","body_bytes_sent":"153","http_referer":"-","http_user_agent":"curl/7.74.0","http_x_forwarded_for":"-","__time__":"1657816609"}
    ```

## 采集模版

了解采集配置结构：[采集配置](../configuration/collection-config.md)

查看更多K8s采集日志模版（从容器中采集文件日等）：<https://github.com/alibaba/ilogtail/blob/main/k8s_templates>

参考更多可组合的日志采集和处理配置样例：<https://github.com/alibaba/ilogtail/blob/main/example_config>
