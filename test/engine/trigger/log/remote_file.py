import argparse
import json as jsonlib
import logging
import random
import time

from logging.handlers import TimedRotatingFileHandler
from datetime import datetime
from faker import Faker
from faker.providers import internet, user_agent, lorem, misc


def apsara(args, logger, faker):
    fileNo = random.randint(1, 1000)
    for i in range(args.count):
        logger.info(f'[{datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")}]\t[{get_random_level()}]\t[{random.randint(1, 10000)}]\t[/build/core/application/Application:{i}]\tfile:file{fileNo}\tlogNo:{i}\tmark:{get_random_mark()}\tmsg:{faker.sentence()}')
        if args.interval > 0:
            time.sleep(args.interval / 1000)

def delimiter(args, logger, faker):
    custom_args = args.custom
    quote = custom_args.get('quote', '')
    delimiter = custom_args.get('delimiter', ' ')
    fileNo = random.randint(1, 1000)
    for i in range(args.count):
        logParts = [
            f'{quote}{get_random_mark()}{quote}',
            f'{quote}file{fileNo}{quote}',
            f'{quote}logNo:{i}{quote}',
            f'{quote}{faker.ipv4()}{quote}',
            f'{quote}{datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")}{quote}',
            f'{quote}{faker.http_method()}{quote}',
            f'{quote}{faker.uri_path()}{quote}',
            f'{quote}HTTP/2.0{quote}',
            f'{quote}{faker.http_status_code()}{quote}',
            f'{quote}{random.randint(1, 10000)}{quote}',
            f'{quote}{faker.user_agent()}{quote}'
        ]
        log = delimiter.join(logParts)
        logger.info(log)
        if args.interval > 0:
            time.sleep(args.interval / 1000)

def delimiterMultiline(args, logger, faker):
    custom_args = args.custom
    quote = custom_args.get('quote', '')
    delimiter = custom_args.get('delimiter', ' ')
    fileNo = random.randint(1, 1000)
    for i in range(args.count):
        logParts = [
            f'{quote}{get_random_mark()}{quote}',
            f'{quote}fi\nle{fileNo}{quote}',
            f'{quote}logNo\n:{i}{quote}',
            f'{quote}{faker.ipv4()}{quote}',
            f'{quote}{datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")}{quote}',
            f'{quote}{faker.http_method()}{quote}',
            f'{quote}{faker.uri_path()}{quote}',
            f'{quote}HT\nTP/2.0{quote}',
            f'{quote}{faker.http_status_code()}{quote}',
            f'{quote}{random.randint(1, 10000)}{quote}',
            f'{quote}{faker.user_agent()}{quote}'
        ]
        log = delimiter.join(logParts)
        logger.info(log)
        if args.interval > 0:
            time.sleep(args.interval / 1000)

def json(args, logger, faker):
    fileNo = random.randint(1, 1000)
    for i in range(args.count):
        logger.info(f'{{"mark":"{get_random_mark()}", "file":"file{fileNo}", "logNo":{i}, "time":"{datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")}", "ip": "{faker.ipv4()}", "method": "{faker.http_method()}", "userAgent": "{faker.user_agent()}", "size": {random.randint(1, 10000)}}}')
        if args.interval > 0:
            time.sleep(args.interval / 1000)

def jsonMultiline(args, logger, faker):
    fileNo = random.randint(1, 1000)
    for i in range(args.count):
        logParts = [
            f'"mark":"{get_random_mark()}"',
            f'"file":"file{fileNo}"',
            f'"logNo":{i}',
            f'"time":"{datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")}"',
            f'"ip": "{faker.ipv4()}"',
            f'"method": "{faker.http_method()}"',
            f'"userAgent": "{faker.user_agent()}"',
            f'"size": {random.randint(1, 10000)}'
        ]
        log = '{' + ',\n'.join(logParts) + '}'
        logger.info(log)
        if args.interval > 0:
            time.sleep(args.interval / 1000)

def regex(args, logger, faker):
    fileNo = random.randint(1, 1000)
    for i in range(args.count):
        logger.info(f'{get_random_mark()} file{fileNo}:{i} {faker.ipv4()} - [{datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")}] "{faker.http_method()} {faker.uri_path()} HTTP/2.0" {faker.http_status_code()} {random.randint(1, 10000)} "{faker.user_agent()}" "{faker.sentence()}"')
        if args.interval > 0:
            time.sleep(args.interval / 1000)

def regexGBK(args, logger, faker):
    fileNo = random.randint(1, 1000)
    for i in range(args.count):
        log = f'{get_random_mark()} file{fileNo}:{i} {faker.ipv4()} - [{datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")}] "{faker.http_method()} {faker.uri_path()} HTTP/2.0" {faker.http_status_code()} {random.randint(1, 10000)} "{faker.user_agent()}" "{faker.sentence()}"'
        logger.info(str(log.encode('gbk')))
        if args.interval > 0:
            time.sleep(args.interval / 1000)

def regexMultiline(args, logger, faker):
    fileNo = random.randint(1, 1000)
    for i in range(args.count):
        multilineLog = '\n'.join(faker.sentences(nb=random.randint(1, 5)))
        logger.info(f'{get_random_mark()} file{fileNo}:{i} [{datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")}] [{get_random_level()}] java.lang.Exception: {multilineLog}')

        if args.interval > 0:
            time.sleep(args.interval / 1000)

def get_random_level():
    return random.choice(['DEBUG', 'INFO', 'WARNING', 'ERROR'])

def get_random_mark():
    return random.choice(['-', 'F'])

def parse_custom_arg_to_dict(custom_arg):
    return jsonlib.loads(custom_arg)

def main():
    parser = argparse.ArgumentParser(description='Log Generator Arg Parser')
    parser.add_argument('--mode', type=str, default='regex', help='Log Type')
    parser.add_argument('--path', type=str, default='default.log', help='Log Path')
    parser.add_argument('--count', type=int, default=100, help='Log Count')
    parser.add_argument('--interval', type=int, default=1, help='Log Interval (ms), < 0 means no interval')
    parser.add_argument('--custom', type=parse_custom_arg_to_dict, help='Custom Args, in the format of json')

    args = parser.parse_args()

    logger = logging.getLogger('log_generator')
    logger.setLevel(logging.INFO)
    # 快速轮转来模拟比较极端的情况
    handler = TimedRotatingFileHandler(args.path, when="s", interval=5, backupCount=3)
    formatter = logging.Formatter('%(message)s')
    handler.setFormatter(formatter)
    handler.flush = lambda: handler.stream.flush()
    logger.addHandler(handler)

    # 随机生成器
    faker = Faker()
    faker.add_provider(internet)
    faker.add_provider(user_agent)
    faker.add_provider(lorem)
    faker.add_provider(misc)

    # 生成数据
    if args.mode == 'apsara':
        apsara(args, logger, faker)
    elif args.mode == 'delimiter':
        delimiter(args, logger, faker)
    elif args.mode == 'delimiterMultiline':
        delimiterMultiline(args, logger, faker)
    elif args.mode == 'json':
        json(args, logger, faker)
    elif args.mode == 'jsonMultiline':
        jsonMultiline(args, logger, faker)
    elif args.mode == 'regex':
        regex(args, logger, faker)
    elif args.mode == 'regexGBK':
        regexGBK(args, logger, faker)
    elif args.mode == 'regexMultiline':
        regexMultiline(args, logger, faker)


if __name__ == '__main__':
    main()
