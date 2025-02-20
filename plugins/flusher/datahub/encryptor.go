package datahub

import (
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"encoding/base64"
	"fmt"
	"io"
	"strings"
)

const (
	secretKey     = " DATAHUB-DWARF1 "
	encryptPrefix = "enc."
)

type Encryptor struct {
	key []byte
}

func NewDefaultEncryptor() *Encryptor {
	return &Encryptor{
		key: []byte(secretKey),
	}
}

func (ep *Encryptor) DecodeKey(accessKeySecret string) (string, error) {
	if !strings.HasPrefix(accessKeySecret, encryptPrefix) {
		return accessKeySecret, nil
	}

	ciphertext, err := base64.StdEncoding.DecodeString(accessKeySecret[len(encryptPrefix):])
	if err != nil {
		return "", err
	}

	text, err := ep.decrypt(ciphertext)
	if err != nil {
		return "", err
	}

	return string(text), nil
}

func (ep *Encryptor) encrypt(plaintext []byte) ([]byte, error) {
	block, err := aes.NewCipher(ep.key)
	if err != nil {
		return nil, err
	}

	ciphertext := make([]byte, aes.BlockSize+len(plaintext))
	iv := ciphertext[:aes.BlockSize]
	if _, err := io.ReadFull(rand.Reader, iv); err != nil {
		return nil, err
	}

	stream := cipher.NewCFBEncrypter(block, iv)
	stream.XORKeyStream(ciphertext[aes.BlockSize:], plaintext)
	return ciphertext, nil
}

func (ep *Encryptor) decrypt(ciphertext []byte) ([]byte, error) {
	block, err := aes.NewCipher(ep.key)
	if err != nil {
		return nil, err
	}

	if len(ciphertext) < aes.BlockSize {
		return nil, fmt.Errorf("ciphertext too short")
	}

	iv := ciphertext[:aes.BlockSize]
	ciphertext = ciphertext[aes.BlockSize:]
	plaintext := make([]byte, len(ciphertext))

	stream := cipher.NewCFBDecrypter(block, iv)
	stream.XORKeyStream(plaintext, ciphertext)
	return plaintext, nil
}
