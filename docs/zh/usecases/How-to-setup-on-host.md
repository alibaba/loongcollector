# iLogtail使用入门-主机环境日志采集到SLS

## 使用前准备

- 开通阿里云账号（仅依赖日志服务写入数据）
- 准备一台具备公网访问权限的X86-64 Linux服务器

## 开通日志服务
**提示：** 日志服务为后付费功能，试用后若删除logstore即不再计费。

1. 登录阿里云后进入控制台([https://console.aliyun.com/](https://homenew.console.aliyun.com/))，在搜索栏输入“SLS”，选择“控制台入口”-“日志服务SLS”，根据提示开通日志服务。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928227338-c5b2601a-f867-47ae-902d-68af6eaa8733.png#clientId=u9feef6e1-fad2-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=129&id=u866f6fdb&margin=%5Bobject%20Object%5D&name=image.png&originHeight=734&originWidth=3250&originalType=binary&ratio=1&rotation=0&showTitle=false&size=836828&status=done&style=none&taskId=u1bc82a6d-d2e0-494a-9047-9c3d6681567&title=&width=570" alt="image.png" width="570"/>

## 创建日志配置

1. 日志服务开通后，跳转到控制台，点击创建project。填入project相关属性，本教程中所属地域选择“华北6（乌兰察布）”（注意若使用阿里云ECS请在选择“所属区域”时与ECS的保持一致）。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928227554-69b5c2d6-7246-4e56-a709-88559fe6cf38.png#clientId=u9feef6e1-fad2-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=686&id=u702e3969&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1568&originWidth=1372&originalType=binary&ratio=1&rotation=0&showTitle=false&size=740590&status=done&style=none&taskId=u8ecfcd67-644c-4c9b-9d76-e4c41b0565f&title=&width=600" alt="image.png" width="600"/>

2. project创建成功后，会提示创建logstore(project和logstore属于包含关系，一个project下可创建多个logstore)，点击创建logstore并按照提示进行配置，输入logstore名称后，点击“确认”。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928227104-0cb20bb1-9b9d-4cff-9bed-a258162ef771.png#clientId=u9feef6e1-fad2-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=190&id=u431cd39f&margin=%5Bobject%20Object%5D&name=image.png&originHeight=428&originWidth=744&originalType=binary&ratio=1&rotation=0&showTitle=false&size=93707&status=done&style=none&taskId=u3a0d351b-a478-44b2-aa3b-0ac73cf3256&title=&width=330" alt="image.png" width="330"/>

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928227258-acb74e40-4def-4eb8-b904-de5b8144b758.png#clientId=u9feef6e1-fad2-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=708&id=uc0cdf571&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1574&originWidth=1356&originalType=binary&ratio=1&rotation=0&showTitle=false&size=636842&status=done&style=none&taskId=u359b0853-951c-4eb7-a214-4921b09d247&title=&width=610" alt="image.png" width="610"/>

3. logstore创建成功后，会提示接入数据，点击“确定”并在“快速数据接入”对话框中搜索“单行”然后选择“单行-文本日志”。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928227044-34090c57-c9c7-4510-9f7b-bf1aeb3af9d9.png#clientId=u9feef6e1-fad2-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=165&id=u05c4cbbb&margin=%5Bobject%20Object%5D&name=image.png&originHeight=368&originWidth=872&originalType=binary&ratio=1&rotation=0&showTitle=false&size=95048&status=done&style=none&taskId=u4ef9f06e-c59c-48bc-be2b-cc13d334384&title=&width=390" alt="image.png" width="390"/>

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928227987-97c48fc7-ff52-48a0-8f79-fb75fd331651.png#clientId=u9feef6e1-fad2-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=187&id=u67ee4295&margin=%5Bobject%20Object%5D&name=image.png&originHeight=582&originWidth=2116&originalType=binary&ratio=1&rotation=0&showTitle=false&size=75732&status=done&style=none&taskId=u05f31067-73bb-4469-84d9-3d7e086976c&title=&width=680" alt="image.png" width="680"/>

4. 进入“极简单行”配置界面，选择自建机器，并将安装命令复制下来用于服务器ilogtail的安装。请保持页面打开。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928228312-75e60971-aba8-49ce-8fc5-433506686014.png#clientId=u9feef6e1-fad2-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=478&id=u9213e35f&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1090&originWidth=1826&originalType=binary&ratio=1&rotation=0&showTitle=false&size=644630&status=done&style=none&taskId=u16612771-f08f-47ea-b21e-44d7835df69&title=&width=800" alt="image.png" width="800"/>

## 安装ilogtail

1. 以root身份登陆Linux服务器，并输入复制的安装命令。该安装命令使用的是预编译的ilogtail二进制包，适用于绝大多数X86-64架构的Linux发行版，安装脚本依赖bash请提前确认安装。（注意此处举例的命令仅适用于Project地域为乌兰察布并使用公网采集的情况）
```bash
$ wget http://logtail-release-cn-wulanchabu.oss-cn-wulanchabu.aliyuncs.com/linux64/logtail.sh -O logtail.sh; chmod 755 logtail.sh; ./logtail.sh install cn-wulanchabu-internet
```
&emsp;&emsp;&emsp;控制台应该打印出类似以下的消息，代表安装成功。
```bash
systemd startup done
ilogtail is running
install logtail success
start logtail success
{
	"UUID" : "A3412556-C126-4D38-8959-AC8BB628F00D",
	"hostname" : "iZhp38z91mt9hkn2ubltn9Z",
	"instance_id" : "BFB9BC78-68A5-11EC-8A7E-00163E00FF4E_172.18.108.207_1640782163",
	"ip" : "172.18.108.207",
	"logtail_version" : "1.0.25",
	"os" : "Linux; 4.18.0-348.2.1.el8_5.x86_64; #1 SMP Tue Nov 16 14:42:35 UTC 2021; x86_64",
	"update_time" : "2021-12-29 20:49:23"
}
```

2. 确认ilogtail正常运行。ilogtail在运行的时候会有两个进程，可通过`**ps -ef | grep ilogtail**`命令查看。同时可调用ilogtail自带的命令查看`**/etc/init.d/ilogtaild status**`。成功执行情况如下
```bash
$ ps -ef | grep logtail
root        1577       1  0 20:49 ?        00:00:00 /usr/local/ilogtail/ilogtail
root        1579    1577  0 20:49 ?        00:00:00 /usr/local/ilogtail/ilogtail
root        1831    1197  0 21:09 pts/0    00:00:00 grep --color=auto logtail
$ ps -ef | grep ilogtail
root        1577       1  0 20:49 ?        00:00:00 /usr/local/ilogtail/ilogtail
root        1579    1577  0 20:49 ?        00:00:00 /usr/local/ilogtail/ilogtail
root        1833    1197  0 21:09 pts/0    00:00:00 grep --color=auto ilogtail
$ /etc/init.d/ilogtaild status
ilogtail is running
```

3. 从阿里云控制台复制帐号ID，创建用户标识文件。（注意不要使用子账号ID）

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928229123-f39c7b58-823d-443c-ae2c-3fba6ee629b5.png#clientId=u9feef6e1-fad2-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=85&id=qzYTz&margin=%5Bobject%20Object%5D&name=image.png&originHeight=462&originWidth=3862&originalType=binary&ratio=1&rotation=0&showTitle=false&size=957307&status=done&style=none&taskId=u750f0af5-bf1c-40d6-8948-4d9a17a8031&title=&width=710" alt="image.png" width="710"/>

```bash
> touch /etc/ilogtail/users/****************
```
## 完成日志配置

1. 回到Web界面，单击页面右下角的“确认安装完毕”。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928228697-7fc71bc3-1455-4443-93ae-0742ec061344.png#clientId=u9feef6e1-fad2-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=47&id=udd3aeb1c&margin=%5Bobject%20Object%5D&name=image.png&originHeight=108&originWidth=1342&originalType=binary&ratio=1&rotation=0&showTitle=false&size=27833&status=done&style=none&taskId=u4a9af37e-d61d-4dc4-9c89-5f4ebbb06a9&title=&width=580" alt="image.png" width="580"/>

2.  跳转到机器组配置界面，按照提示填写表格，其中IP地址栏填写Linux服务器的地址，配置完成后点击“下一步”。（若服务器有多个ip地址请填写安装ilogtail时回显的ip地址，也可以通过`cat /usr/local/ilogtail/app_info.json`获得）

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928229441-dec72af4-f941-4398-bcc3-81442cbe7cf6.png#clientId=u9feef6e1-fad2-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=470&id=u071d26e6&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1122&originWidth=1672&originalType=binary&ratio=1&rotation=0&showTitle=false&size=381586&status=done&style=none&taskId=u0c605315-e3a1-443f-9549-6a99320e4fc&title=&width=700" alt="image.png" width="700"/>

3. 跳转到选择机器组界面。勾选刚刚创建的机器组，点击“>”加入应用机器组，然后点击“下一步”进入ilogtail配置。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928229674-9ec1fde4-b85d-4ae3-84ee-37306dd3e65d.png#clientId=u9feef6e1-fad2-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=470&id=uc012bfb9&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1170&originWidth=1744&originalType=binary&ratio=1&rotation=0&showTitle=false&size=281029&status=done&style=none&taskId=u7f70e7a4-1d05-40aa-89bb-cdd10d1c137&title=&width=700" alt="image.png" width="700"/>

4. 在ilogtail配置中仅修改“配置名称”和“日志路径”两个必填项，点击“下一步”确认。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928230043-be9449f2-5e97-499c-aeb0-37de2d8c0471.png#clientId=u9feef6e1-fad2-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=584&id=ufcc8c42f&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1418&originWidth=1652&originalType=binary&ratio=1&rotation=0&showTitle=false&size=776171&status=done&style=none&taskId=u95078439-d442-4b7b-8858-00f1f9a350b&title=&width=680" alt="image.png" width="680"/>

5. 完成索引配置。这一步不对任何选项进行修改，直接点击下一步完成配置。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928231842-574229ca-6cd4-45d9-9899-8e78c0336bde.png#clientId=u9feef6e1-fad2-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=458&id=ue74f3d02&margin=%5Bobject%20Object%5D&name=image.png&originHeight=1292&originWidth=2398&originalType=binary&ratio=1&rotation=0&showTitle=false&size=634374&status=done&style=none&taskId=ua0ba35d9-b145-4cb7-bf1f-d196c3482f5&title=&width=850" alt="image.png" width="850"/>

&emsp;&emsp;&emsp;此时，整个日志配置已经完成。请保持页面打开。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928231310-d80a1232-d04b-462d-af53-f6e053e16304.png#clientId=u9feef6e1-fad2-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=113&id=ufe830a35&margin=%5Bobject%20Object%5D&name=image.png&originHeight=272&originWidth=1448&originalType=binary&ratio=1&rotation=0&showTitle=false&size=158609&status=done&style=none&taskId=u5001bc2c-d73c-4617-b400-cc7fa12541f&title=&width=600" alt="image.png" width="600"/>

## 上报日志并查看

1. 登陆Linux服务器，输入如下命令持续生成日志。
```bash
$ while true; do echo $(date) >>/tmp/demo.log; sleep 10; done
```

2. 回到Web控制台，点击配置完成界面的“查询日志”跳转到日志查询界面。点击页面左侧的“放大镜”图标，选中logstore，点击“眼睛”图标，在左侧出现的“消费预览”侧边栏中尝试调整Shard和时间范围，点击预览查看上报的日志。

<img src="https://cdn.nlark.com/yuque/0/2021/png/22577445/1640928232509-b6a1080b-21c9-44b9-a73e-9886e87f6635.png#clientId=u9feef6e1-fad2-4&crop=0&crop=0&crop=1&crop=1&from=paste&height=238&id=u5eb6e6b8&margin=%5Bobject%20Object%5D&name=image.png&originHeight=628&originWidth=2638&originalType=binary&ratio=1&rotation=0&showTitle=false&size=571941&status=done&style=none&taskId=uc23092d3-61f0-48e3-ab82-0398ec8e02a&title=&width=1000" alt="image.png" width="1000"/>

## What's Next
&emsp;&emsp;&emsp;你可以进入下一节学习《[ilogtail使用入门-K8S环境日志采集到SLS](./How-to-setup-in-k8s-environment.md)》

&emsp;&emsp;&emsp;了解主机采集原理《[Logtail采集原理](https://help.aliyun.com/document_detail/89928.html)》

&emsp;&emsp;&emsp;也可以学习ilogtail采集的更多用法：

   - [采集文本日志](https://help.aliyun.com/document_detail/65020.html)​
   - ​[使用Logtail插件采集数据](https://help.aliyun.com/document_detail/95924.html)
   - [使用Logtail插件处理数据](https://help.aliyun.com/document_detail/196153.html)
