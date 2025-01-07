import argparse
import mmap
import os

def main():
    parser = argparse.ArgumentParser(description='mmap')
    parser.add_argument('--commandCnt', type=int, default=10, help='command count')
    parser.add_argument('--filename', type=str, default='/tmp/loongcollector/ebpfFileSecurityHook3.log', help='filename')

    args = parser.parse_args()

    with open(args.filename, 'w') as f:
        fd = f.fileno()
        for i in range(args.commandCnt):
            mm = mmap.mmap(fd, 20, prot=mmap.PROT_READ | mmap.PROT_WRITE, flags=mmap.MAP_SHARED)
            mm.close()
    
    os.remove(args.filename)


if __name__ == '__main__':
    main()
