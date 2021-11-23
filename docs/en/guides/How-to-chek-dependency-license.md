# How to check the license of the dependent package?

iLogtail is open source collector based on the Apache 2.0 license. Developers need to ensure that the dependent package license is compatible with the Apache 2.0 license. All dependent packages or source code import license are located in the root directory `licenses` folder.

## Check the Dependency Package License

Developers can use the following command to scan the dependent package license.

```makefile
make check-dependency-licenses
```

- When the prompt `DEPENDENCIES CHECKING IS PASSED`, it means that the dependency package check passed.
- When the prompt `FOLLOWING DEPENDENCIES SHOULD BE ADDED to LICENSE_OF_ILOGTAIL_DEPENDENCIES.md`, it means that the dependent package will need to be added (you need to ensure that the license is compatible with the Apache 2.0 license), you can view the file `find_licenses/LICENSE-{liencese type}` to adding the dependency package license.
- When the prompt `FOLLOWING DEPENDENCIES IN LICENSE_OF_ILOGTAIL_DEPENDENCIES.md SHOULD BE REMOVED`, it means that there are redundant dependent packages, just delete them as prompted.