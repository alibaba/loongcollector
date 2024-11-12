import argparse
import json
import logging
import math
import random
import time

from logging.handlers import TimedRotatingFileHandler
from datetime import datetime
from faker import Faker
from faker.providers import internet, user_agent, lorem, misc

BATCH_SIZE = 100

def nginx(args, logger, faker):
    startTime = time.perf_counter()
    exampleLog = ''
    for _ in range(BATCH_SIZE):
        exampleLog += f'{faker.ipv4()} - - [{datetime.now().strftime("%d/%b/%Y:%H:%M:%S %z")}] "{faker.http_method()} {faker.url()} HTTP/1.1" {faker.http_status_code()} {random.randint(1, 10000)} "{faker.url()}" "{faker.user_agent()}\n"'
    randomLogCost = (time.perf_counter() - startTime) / BATCH_SIZE
    writeTimePerSecond = math.floor(args.rate * 1024 * 1024 / (len(exampleLog.encode('utf-8'))))
    sleepInterval = 1 / writeTimePerSecond - randomLogCost

    startTime = datetime.now()
    while True:
        now = datetime.now()
        fakeLog = f'{faker.ipv4()} - - [{now.strftime("%d/%b/%Y:%H:%M:%S %z")}] "{faker.http_method()} {faker.url()} HTTP/1.1" {faker.http_status_code()} {random.randint(1, 10000)} "{faker.url()}" "{faker.user_agent()}"\n' * BATCH_SIZE
        logger.info(fakeLog[:-1])
        if sleepInterval > 0:
            start = time.perf_counter()
            while (time.perf_counter() - start) < sleepInterval:
                pass
        if (now - startTime).seconds > args.duration * 60:
            break

def parse_custom_arg_to_dict(custom_arg):
    return json.loads(custom_arg)

def main():
    parser = argparse.ArgumentParser(description='Log Generator Arg Parser')
    parser.add_argument('--mode', type=str, default='nginx', help='Log Type')
    parser.add_argument('--path', type=str, default='default.log', help='Log Path')
    parser.add_argument('--rate', type=int, default=10, help='Log Generate Rate (MB/s)')
    parser.add_argument('--duration', type=int, default=60, help='Log Generate Duration (min)')
    parser.add_argument('--custom', nargs='*', type=parse_custom_arg_to_dict, help='Custom Args, in the format of key=value')

    args = parser.parse_args()

    logger = logging.getLogger('log_generator')
    logger.setLevel(logging.INFO)
    # 快速轮转来模拟比较极端的情况
    handler = TimedRotatingFileHandler(args.path, when="s", interval=70, backupCount=3)
    formatter = logging.Formatter('%(message)s')
    handler.setFormatter(formatter)
    logger.addHandler(handler)

    # 随机生成器
    faker = Faker()
    faker.add_provider(internet)
    faker.add_provider(user_agent)
    faker.add_provider(lorem)
    faker.add_provider(misc)

    # 生成数据
    if args.mode == 'nginx':
        nginx(args, logger, faker)


if __name__ == '__main__':
    main()
