# Why do we need override some source codes?

1. Some package codes cannot compile on windows 386 because of Go const, more details please see the following docs.
    1. https://github.com/go-mysql-org/go-mysql/issues/314
    2. https://github.com/golang/go/issues/19621#issuecomment-287858122
2. The C files of some package cannot be included by Go vendor.
3. Some package is too heavy, we want to reduce the dependency packages.
