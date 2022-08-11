# 发布版本

1. 查看根目录 `CHANGELOG.md` 是否含有Unreleased 内容。
2. 执行`sh scripts/gen_release_markdown.sh {version} {milestone-number}`, milestone-number 可以点击 [这里](https://github.com/alibaba/ilogtail/milestones) 查看。比如v1.0.28 milestone number 为 [2](https://github.com/alibaba/ilogtail/milestone/2)，则执行`sh scripts/gen_release_markdown.sh 1.0.28 2`。
3. 检查生成于`changes/{version}.md`的发布文档是否含有第一点所述全部待发布内容。
3. 检测生成的下载链接是否可以访问发布文件，如果不可以访问，进行OSS 发布版本上传。
4. 检查根目录 `CHANGELOG.md` Unreleased 部分是否已经删除。
5. Commit & Push github。
6. 界面操作 Release 发布，发布内容复制`changes/{version}.md`。
