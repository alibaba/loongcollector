# How to release?
1. Please check whether `CHANGELOG.md` in root dir has unreleased contents.
2. Execute `sh scripts/gen_release_markdown.sh {version} {milestone-number}` commends to generate release markdown. And milestone-number cloud be found at [the link](https://github.com/alibaba/ilogtail/milestones), such as the milestone number of v1.0.28 is [2](https://github.com/alibaba/ilogtail/milestone/2).
3. Please check whether the file named `changes/{version}.md` has all unreleased contents.
4. Please check whether the download link in `changes/{version}.md` working well. If having broken links, please upload the release files.
5. Commit & Push Github.
6. Press the release button in the GitHub console and paste the content of `changes/{version}.md` to here.
