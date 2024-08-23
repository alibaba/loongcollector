package control

import (
	"context"

	"github.com/alibaba/ilogtail/test/config"
)

func SetQuery(ctx context.Context, sql string) (context.Context, error) {
	return context.WithValue(ctx, config.QueryKey, sql), nil
}

func GetQuery(ctx context.Context) string {
	value := ctx.Value(config.QueryKey)
	if value == nil {
		return "*"
	}
	return value.(string)
}
