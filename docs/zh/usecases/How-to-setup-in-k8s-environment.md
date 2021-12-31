# iLogtail使用入门-K8S环境日志采集到SLS

## 使用前准备

- 开通阿里云日志服务并创建了Project（具体步骤参见上一节《[ilogtail使用入门-主机环境日志采集到SLS](#)》）
- 准备一个具备公网访问权限的K8S集群，服务器架构为X86-64。

## 创建日志配置

1. 跳转到日志服务控制台([sls.console.aliyun.com](https://sls.console.aliyun.com))，点击上一节中已经创建的project。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928312488-82585d1b-fcc7-4721-be2e-38b5abe2a225.png#clientId=u38ae2f00-0bbf-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=206&id=u084c6196&margin=%5Bobject%20Object%5D&name=image.png&originHeight=528&originWidth=2176&originalType=binary&ratio=1&rotation=0&showTitle=false&size=273610&status=done&style=none&taskId=u246c8949-630b-42c8-9533-62598bc8fd7&title=&width=850" alt="image.png" width="850"/>

2. 进入Project查询页面后，点击左侧边栏的“放大镜”图标，展开logstore管理界面，点击“+”，弹出“创建Logstore”右侧边栏。按照提示进行配置，输入logstore名称后，点击“确认”。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928312660-9ca0cdd6-7775-46f6-8096-2e61b2586796.png#clientId=u38ae2f00-0bbf-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=622&id=udcbf76eb&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1514&originWidth=2068&originalType=binary&ratio=1&rotation=0&showTitle=false&size=999672&status=done&style=none&taskId=uf871982d-358b-4f49-9f6f-7624f208cfe&title=&width=850" alt="image.png" width="850"/>

3. logstore创建成功后，取消数据接入向导。点击左侧边栏中的“立方体”按钮，在弹出的“资源”浮层中选择“机器组”。在展开的“机器组”左边栏中，点击右上角的“四方格”图标，在弹出的浮层中选择“创建机器组”。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928312206-704068be-b346-4617-9723-275da8f1f3ab.png#clientId=u38ae2f00-0bbf-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=260&id=u526d2f42&margin=%5Bobject%20Object%5D&name=image.png&originHeight=622&originWidth=814&originalType=binary&ratio=1&rotation=0&showTitle=false&size=69563&status=done&style=none&taskId=u90eff062-d8df-4824-ac78-fd3f79ba2b0&title=&width=340" alt="image.png" width="340"/>

4. 在“创建机器组”有侧边栏中按提示配置，“机器组标识”选择“用户自定义标识”，“名称”、“机器组Topic”、“用户自定义标识”建议保持一致。“用户自定义标识”是其中最为重要的一个配置，本教程中使用“my-k8s-group”，后续在安装ilogtail时会再次用到。“点击”确认保存机器组。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928312409-cdeb2ef6-d358-484b-98c7-c73903bd44e0.png#clientId=u38ae2f00-0bbf-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=369&id=ubb676ad0&margin=%5Bobject%20Object%5D&name=image.png&originHeight=874&originWidth=1420&originalType=binary&ratio=1&rotation=0&showTitle=false&size=86304&status=done&style=none&taskId=u43920daa-a9bf-4e4f-b631-2b807e940f1&title=&width=600" alt="image.png" width="600"/>

5. 再次点击左侧边栏的“放大镜”图标，展开logstore管理界面，点击第2步中创建的logstore的“向下展开”图标，弹出“配置Logstore”菜单。点击“logtail配置”的“+”按钮。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928312430-ce0e158a-f9f3-4e1a-8cf8-ca7ee162e40f.png#clientId=u38ae2f00-0bbf-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=274&id=ud09ccc24&margin=%5Bobject%20Object%5D&name=image.png&originHeight=630&originWidth=644&originalType=binary&ratio=1&rotation=0&showTitle=false&size=75835&status=done&style=none&taskId=ud843f69c-7847-4268-a3ab-538f2117e19&title=&width=280" alt="image.png" width="280"/>

6. 在弹出的“快速接入数据”对话框中搜索“kube”，并选择“Kubernertes-文件”。在弹出的“提示”框中单机“继续”。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928313341-b12d8cb1-20e5-438f-8246-8763f2e7d403.png#clientId=u38ae2f00-0bbf-4&crop=0&crop=0&crop=0.502&crop=1&from=paste&height=95&id=u88a2ee54&margin=%5Bobject%20Object%5D&name=image.png&originHeight=494&originWidth=2698&originalType=binary&ratio=1&rotation=0&showTitle=false&size=89675&status=done&style=none&taskId=u233c76c9-fb0e-4315-89a9-da7343ce358&title=&width=520" alt="image.png" width="520"/>

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928313245-a630753e-2405-4f1b-988c-147bbc776af5.png#clientId=u38ae2f00-0bbf-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=200&id=Q6WcH&margin=%5Bobject%20Object%5D&name=image.png&originHeight=468&originWidth=1030&originalType=binary&ratio=1&rotation=0&showTitle=false&size=51921&status=done&style=none&taskId=u1617959a-ea23-411d-a484-5ec41d6f09d&title=&width=440" alt="image.png" width="440"/>

7. 在“Kubernertes文件”配置界面，直接选择“使用现有机器组”。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928313539-ad1aa100-dad0-40ae-8544-4aebec96d8df.png#clientId=u38ae2f00-0bbf-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=152&id=b7Z6h&margin=%5Bobject%20Object%5D&name=image.png&originHeight=356&originWidth=1524&originalType=binary&ratio=1&rotation=0&showTitle=false&size=83408&status=done&style=none&taskId=u7d5a3329-846f-4266-b108-c3cfc3e2a2e&title=&width=650" alt="image.png" width="650"/>

8. 跳转到“机器组配置”界面，选择第4步中创建的机器组，点击“>”按钮将其加入到“应用机器组”中，然后点击“下一步”。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928313993-59be012b-68df-44f0-978f-e6607278d48d.png#clientId=u38ae2f00-0bbf-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=678&id=u91ff5ee3&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1574&originWidth=1764&originalType=binary&ratio=1&rotation=0&showTitle=false&size=150122&status=done&style=none&taskId=u457e113d-0b6c-4a7c-849c-1340262be58&title=&width=760" alt="image.png" width="760"/>

9. 在ilogtail配置中仅修改“配置名称”和“日志路径”两个必填项，点击“下一步”确认。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928314848-3a54c0ff-ca69-4d38-8c8d-b80f6b23e9c8.png#clientId=u38ae2f00-0bbf-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=974&id=u99dd5590&margin=%5Bobject%20Object%5D&name=image.png&originHeight=2104&originWidth=1642&originalType=binary&ratio=1&rotation=0&showTitle=false&size=504835&status=done&style=none&taskId=udbde915d-e590-4b83-8c9b-e9f145388b9&title=&width=760" alt="image.png" width="760"/>

10. 完成索引配置。这一步不对任何选项进行修改，直接点击下一步完成配置。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928315212-a2742dd6-b05d-4526-8706-d89dc259df61.png#clientId=u38ae2f00-0bbf-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=458&id=scIzv&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1292&originWidth=2398&originalType=binary&ratio=1&rotation=0&showTitle=false&size=634374&status=done&style=none&taskId=u06a025d5-088b-4656-b91a-97722442a14&title=&width=850" alt="image.png" width="850"/>

&emsp;&emsp;&emsp;此时，整个日志配置已经完成。请保持页面打开。
<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928314475-e73d44c6-b612-4f26-ae92-601c09b98f01.png#clientId=u38ae2f00-0bbf-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=113&id=c92on&margin=%5Bobject%20Object%5D&name=image.png&originHeight=272&originWidth=1448&originalType=binary&ratio=1&rotation=0&showTitle=false&size=158609&status=done&style=none&taskId=u68ab68b2-34c3-47af-85a8-02197b4bd2c&title=&width=600" alt="image.png" width="600"/>

## 安装ilogtail

1. 登陆可以控制K8S集群的中控机。编辑ilogtail的ConfigMap YAML。
```bash
$ vim alicloud-log-config.yaml
```
&emsp;&emsp;&emsp;在Vim中粘贴如下内容并保存（注意，修改注释中提示的字段，7-11行）。
```yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: alibaba-log-configuration
  namespace: kube-system
data:
    log-project: "my-project" #修改为实际project名称
    log-endpoint: "cn-wulanchabu.log.aliyuncs.com" #修改为实际endpoint
    log-machine-group: "my-k8s-group" #可以自定义机器组名称
    log-config-path: "/etc/ilogtail/conf/cn-wulanchabu_internet/ilogtail_config.json" #修改cn-wulanchabu为实际project地域
    log-ali-uid: "*********" #修改为阿里云UID
    access-key-id: "" #本教程用不上
    access-key-secret: "" #本教程用不上
    cpu-core-limit: "2"
    mem-limit: "1024"
    max-bytes-per-sec: "20971520"
    send-requests-concurrency: "20"
```

2. 计算alicloud-log-config.yaml的sha256 hash，并编辑ilogtail的DaemonSet YAML。
```bash
$ sha256sum alicloud-log-config.yaml
f370df37916797aa0b82d709ae6bfc5f46f709660e1fd28bb49c22da91da1214  alicloud-log-config.yaml
$ vim logtail-daemonset.yaml
```
&emsp;&emsp;&emsp;在Vim中粘贴如下内容并保存（注意，修改注释中提示的字段，21、25行）。
```yaml
apiVersion: apps/v1
kind: DaemonSet
metadata:
  name: logtail-ds
  namespace: kube-system
  labels:
    k8s-app: logtail-ds
spec:
  selector:
    matchLabels:
      k8s-app: logtail-ds
  updateStrategy:
    type: RollingUpdate
  template:
    metadata:
      labels:
        k8s-app: logtail-ds
        kubernetes.io/cluster-service: "true"
        version: v1.0
      annotations:
        checksum/config: f370df37916797aa0b82d709ae6bfc5f46f709660e1fd28bb49c22da91da1214 #必须修改为alicloud-log-config.yaml的hash
    spec:
      containers:
      - name: logtail
        image: registry.cn-wulanchabu.aliyuncs.com/log-service/logtail:latest #可以修改为距离k8s集群最近的地域
        resources:
          limits:
            cpu: 2
            memory: 1024Mi
          requests:
            cpu: 100m
            memory: 256Mi
        livenessProbe:
          httpGet:
            path: /liveness
            port: 7953
            scheme: HTTP
          initialDelaySeconds: 30
          periodSeconds: 60
        securityContext:
          privileged: true
        env:
          - name: HTTP_PROBE_PORT
            value: "7953"
          - name: "ALIYUN_LOGTAIL_CONFIG"
            valueFrom:
              configMapKeyRef:
                name: alibaba-log-configuration
                key: log-config-path
          - name: "ALIYUN_LOGTAIL_USER_ID"
            valueFrom:
              configMapKeyRef:
                name: alibaba-log-configuration
                key: log-ali-uid
          - name: "ALIYUN_LOGTAIL_USER_DEFINED_ID"
            valueFrom:
              configMapKeyRef:
                name: alibaba-log-configuration
                key: log-machine-group
          - name: "ALICLOUD_LOG_ACCESS_KEY_ID"
            valueFrom:
              configMapKeyRef:
                name: alibaba-log-configuration
                key: access-key-id
          - name: "ALICLOUD_LOG_ACCESS_KEY_SECRET"
            valueFrom:
              configMapKeyRef:
                name: alibaba-log-configuration
                key: access-key-secret
          - name: "ALICLOUD_LOG_DOCKER_ENV_CONFIG"
            value: "true"
          - name: "ALICLOUD_LOG_ECS_FLAG"
            value: "false"
          - name: "ALICLOUD_LOG_DEFAULT_PROJECT"
            valueFrom:
              configMapKeyRef:
                name: alibaba-log-configuration
                key: log-project
          - name: "ALICLOUD_LOG_ENDPOINT"
            valueFrom:
              configMapKeyRef:
                name: alibaba-log-configuration
                key: log-endpoint
          - name: "ALICLOUD_LOG_DEFAULT_MACHINE_GROUP"
            valueFrom:
              configMapKeyRef:
                name: alibaba-log-configuration
                key: log-machine-group
          - name: "ALIYUN_LOG_ENV_TAGS"
            value: "_node_name_|_node_ip_"
          - name: "_node_name_"
            valueFrom:
              fieldRef:
                fieldPath: spec.nodeName
          - name: "_node_ip_"
            valueFrom:
              fieldRef:
                fieldPath: status.hostIP
          # resource limit for logtail self process
          - name: "cpu_usage_limit"
            valueFrom:
              configMapKeyRef:
                name: alibaba-log-configuration
                key: cpu-core-limit
          - name: "mem_usage_limit"
            valueFrom:
              configMapKeyRef:
                name: alibaba-log-configuration
                key: mem-limit
          - name: "max_bytes_per_sec"
            valueFrom:
              configMapKeyRef:
                name: alibaba-log-configuration
                key: max-bytes-per-sec
          - name: "send_request_concurrency"
            valueFrom:
              configMapKeyRef:
                name: alibaba-log-configuration
                key: send-requests-concurrency
        volumeMounts:
        - name: sock
          mountPath: /var/run/
        - name: root
          mountPath: /logtail_host
          readOnly: true
          mountPropagation: HostToContainer
      terminationGracePeriodSeconds: 30
      tolerations:
      - operator: "Exists"
      hostNetwork: true
      dnsPolicy: "Default"
      volumes:
      - name: sock
        hostPath:
          path: /var/run/
      - name: root
        hostPath:
          path: /
```

3. 应用YAML配置，创建ConfigMap和DaemonSet。
```bash
$ kubectl apply -f alicloud-log-config.yaml
configmap/alibaba-log-configuration created
$ kubectl apply -f logtail-daemonset.yaml
daemonset.apps/logtail-ds created
```

4. 等待1分钟，检查DeamonSet是否正常运行
```bash
$ kubectl get -f logtail-daemonset.yaml
```
&emsp;&emsp;&emsp;这个时候控制台应该打印出类似以下的消息，代表安装成功
```bash
NAME         DESIRED   CURRENT   READY   UP-TO-DATE   AVAILABLE   NODE SELECTOR   AGE
logtail-ds   3         3         3       3            3           <none>          2m1s
```
## 上报日志并查看

1. 创建一个用于持续生成日志的Pod。
```bash
$ vim demo-pod.yaml
```
&emsp;&emsp;&emsp;在Vim中粘贴如下内容并保存（注意，可能需要修改注释中提示的字段，8-9行）。
```yaml
apiVersion: v1
kind: Pod
metadata:
  labels:
    name: demo-pod
  name: demo-pod
spec:
#  imagePullSecrets:          # Comment out to enable specific image pull secret
#    - name: myregistrykey    # repleace it to specific registry key containers
containers:
    - image: busybox
      imagePullPolicy: IfNotPresent
      name: demo-pod
      command: ["/bin/sh"]
      args: ["-c", "while true; do echo $(date) >>/tmp/demo.log; sleep 10; done"]
      resources: {}
      securityContext:
        capabilities: {}
        privileged: false
      terminationMessagePath: /dev/termination-log
  dnsPolicy: ClusterFirst
  restartPolicy: Always
```
&emsp;&emsp;&emsp;应用YAML配置，创建Pod
```bash
$ kubectl apply -f demo-pod.yaml
pod/demo-pod created
```

2. 回到Web控制台，点击配置完成界面的“查询日志”跳转到日志查询界面。点击页面左侧的“放大镜”图标，选中logstore，点击“眼睛”图标，在左侧出现的“消费预览”侧边栏中尝试调整Shard和时间范围，点击预览查看上报的日志。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928314378-55c2dbdb-f0f4-4dc5-a6c5-cc842d99adeb.png#clientId=u38ae2f00-0bbf-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=228&id=u7e2712db&margin=%5Bobject%20Object%5D&name=image.png&originHeight=520&originWidth=638&originalType=binary&ratio=1&rotation=0&showTitle=false&size=62464&status=done&style=none&taskId=uc64ee29e-c75a-43da-9dc3-f246f3bee18&title=&width=280" alt="image.png" width="280"/>

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928315676-7d4d93ed-0b3d-4406-8ac5-5c57d38f3c51.png#clientId=u38ae2f00-0bbf-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=340&id=u2d157d21&margin=%5Bobject%20Object%5D&name=image.png&originHeight=780&originWidth=1376&originalType=binary&ratio=1&rotation=0&showTitle=false&size=109988&status=done&style=none&taskId=ub304321b-1a4c-4f8d-891b-aa056f8841b&title=&width=600" alt="image.png" width="600"/>

## What's Next
&emsp;&emsp;&emsp;你可以进入下一节学习《[iLogtail本地配置模式部署(For Kafka Flusher)](./How-to-local-deploy.md)》

&emsp;&emsp;&emsp;了解容器采集原理《[通过DaemonSet-控制台方式采集容器标准输出](https://help.aliyun.com/document_detail/66658.html)》

&emsp;&emsp;&emsp;也可以学习ilogtail采集的更多用法：

   - [采集容器日志](https://help.aliyun.com/document_detail/95925.html)
   - [如何获取容器的Label和环境变量](https://help.aliyun.com/document_detail/354831.html)
