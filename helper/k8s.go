package helper

import (
	"regexp"
	"strconv"
)

var deploymentPodReg = regexp.MustCompile(`^([\w\-]+)\-([0-9a-z]{9,10})\-([0-9a-z]{5})$`)
var setPodReg = regexp.MustCompile(`^([\w\-]+)\-([0-9a-z]{5})$`)
var statefulSetPodReg = regexp.MustCompile(`^([\w\-]+)\-(\d+)$`)

func ExtractPodWorkload(name string) string {
	switch {
	case deploymentPodReg.MatchString(name):
		index := deploymentPodReg.FindStringSubmatchIndex(name)
		return name[index[2]:index[3]]
	case setPodReg.MatchString(name):
		index := setPodReg.FindStringSubmatchIndex(name)
		return name[index[2]:index[3]]
	case statefulSetPodReg.MatchString(name):
		index := statefulSetPodReg.FindStringSubmatchIndex(name)
		return name[index[2]:index[3]]
	}
	return name
}

func ExtractStatefulSetNum(pod string) int {
	if statefulSetPodReg.MatchString(pod) {
		index := statefulSetPodReg.FindStringSubmatchIndex(pod)
		num := pod[index[4]:index[5]]
		n, _ := strconv.Atoi(num)
		return n
	}
	return -1
}
