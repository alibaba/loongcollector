# 定义最大文件大小
MAX_SIZE=$((5 * 1024 * 1024))

# 持续运行的循环
while true; do
  # 查找匹配 /home/*/test.out 的文件
  for file in $(find /home -type f -name "test.out"); do
    # 获取文件大小
    filesize=$(stat -c "%s" "$file")
    
    # 如果文件大小超过限制，则清空文件
    if [ "$filesize" -gt "$MAX_SIZE" ]; then
      echo "Clearing file: $file (Size: $filesize bytes)"
      > "$file" # 清空文件内容
    fi
  done

  # 每秒检查一次
  sleep 1
done