# 开发指南

## 开发

* 添加接口

接口统一添加到 `router` 文件夹下的 `router.go` 文件中，并在 `manger` 和 `interface` 文件夹的相应位置添加实现接口的代码。

* 增加存储适配

若想扩展存储方式，例如实现 `mysql` 存储等，可以在 `store` 文件夹下实现已有的数据库接口 `interface_database` ，并在 `store.go` 的 `newStore` 函数中添加对应的存储类型。

## 编译

进入 Config Server 的 `service` 文件夹下

``` bash
go build -o ConfigServer
```

## 测试

测试文件统一存放在 `test` 文件夹下。

可以通过如下方式查看测试覆盖率：

```bash
go test -cover github.com/alibaba/ilogtail/config_server/service/test -coverpkg github.com/alibaba/ilogtail/config_server/service/... -coverprofile coverage.out -v
go tool cover -func=coverage.out -o coverage.txt
# Visualization results: go tool cover -html=coverage.out -o coverage.html
cat coverage.txt
```
