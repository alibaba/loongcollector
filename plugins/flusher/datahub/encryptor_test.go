package datahub

import (
	"encoding/base64"
	"fmt"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestEncrypt(t *testing.T) {
	encrypotr := &Encryptor{
		key: []byte("12345678901234567890123456789012"),
	}

	str := "This is a message"
	res1, err := encrypotr.encrypt([]byte(str))
	assert.Nil(t, err)

	res2, err := encrypotr.encrypt([]byte(str))
	assert.Nil(t, err)

	// 每次生成的结果都不同
	assert.NotEqual(t, res1, res2)
	fmt.Println(base64.StdEncoding.EncodeToString(res1))
	fmt.Println(base64.StdEncoding.EncodeToString(res2))

	text1, err := encrypotr.decrypt(res1)
	assert.Nil(t, err)

	text2, err := encrypotr.decrypt(res2)
	assert.Nil(t, err)

	assert.Equal(t, text1, text2)
}
