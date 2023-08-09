# Git commit log

## 提供者

[LH0310](https://github.com/LH0310)

## 描述

`git commit log` 记录了一个 Git 仓库中所有提交的历史信息。每一个 Git 仓库都包含了自己的提交日志，这些信息通常可以通过 `git log` 命令进行查看。当执行新的提交操作时，Git 会自动更新这个日志。

由于 Git 本身没有显式地存储这个信息，为了方便在项目中直接访问并接入日志系统，下面提供一个利用 Git 钩子存储提交信息的方法。

为了设置这个钩子，需要在 `.git/hooks` 目录中创建一个名为 `post-commit` 的文件，并确保它是可执行的。然后在脚本中写入如下内容：

``` sh
#!/bin/sh
git log -1 >> /path/to/your/log/gitlog.txt
```

这个 Git 钩子（hooks）会在每次提交后自动将最新的一次提交信息追加到gitlog.txt中。

**注意**，上面的方法只是一个示例，无法准确应对多个分支和多个合作者的情况，在实际项目中要根据自身情况进行相应的配置。

## 日志输入样例

``` 
commit 73669da2a51694cac0563fd1c93a79394bfc2e60
Author: linrunqi08 <90741255+linrunqi08@users.noreply.github.com>
Date:   Thu Jul 6 19:14:56 2023 +0800

    raw http server support v1 (#975)
    
    * add raw http server v1
    
    * fix compile
    
    * add test

commit 1ecb55c0fac8154c0638e715f383ec81b4083b03
Author: wangpeix <105137931+wangpeix@users.noreply.github.com>
Date:   Wed Jun 28 22:13:05 2023 +0800

    add use-case (#954)
    
    Co-authored-by: wangpeix <luckywangpei@didiglobal.com>

commit c73284a6665e93f3b69ed1a4b199743e638b73bd
Author: yyuuttaaoo <yyuuttaaoo@gmail.com>
Date:   Wed Jun 21 14:53:31 2023 +0800

    add empty example file
```

## 日志输出样例

``` json
2023-07-12 03: 14: 47 {
    "hash": "d7e976d10c4bc8560726dfe74eeb103c3597ab34",
    "author": "JsongHu <jsong23531@gmail.com>",
    "date": "Sat Jul 8 01:30:50 2023 +0800",
    "message": "    change devcontainer.json to fit windows\n",
    "__time__": "1689131686"
}
2023-07-12 03: 14: 47 {
    "hash": "73669da2a51694cac0563fd1c93a79394bfc2e60",
    "author": "linrunqi08 <90741255+linrunqi08@users.noreply.github.com>",
    "date": "Thu Jul 6 19:14:56 2023 +0800",
    "message": "    raw http server support v1 (#975)\n    \n    * add raw http server v1\n    \n    * fix compile\n    \n    * add test\n",
    "__time__": "1689131686"
}
2023-07-12 03: 14: 47 {
    "hash": "6d435e716820b2ee29670b808101d60a7935220e",
    "author": "Bingchang Chen <abingcbc626@gmail.com>",
    "date": "Mon Jul 3 20:39:39 2023 +0800",
    "message": "    fix get nanosecond (#970)",
    "__time__": "1689131686"
}
```

## 采集配置

``` yaml
enable: true
inputs:
  - Type: file_log
    LogPath: /path/to/your/log/
    FilePattern: gitlog.txt
processors:
  - Type: processor_split_log_regex
    SplitRegex: ^commit [0-9a-fA-F]{4,40}$
    SplitKey: content
  - Type: processor_regex
    SourceKey: content
    Regex: commit\s+([0-9a-fA-F]{4,40})\nAuthor:\s+(.+?)\nDate:\s+(.+?)\n\n([\s\S]+)
    Keys:
      - hash
      - author
      - date
      - message
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```
