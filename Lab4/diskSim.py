import sys
import random

NUM_CYLS = 5000
NUM_RAND_REQS = 100

def read_sequence(fn):
    pass

def generate_random_sequence():
    pass

def fcfs(seq, pos):
    pass

def sstf(seq, pos):
    pass

def scan(seq, pos, direction):
    pass

def cscan(seq, pos, direction):
    pass

def look(seq, pos, direction):
    pass

def clook(seq, pos, direction):
    pass

def main():
    if len(sys.argv) < 2:
        print('Error: Must provide at least one argument')
        sys.exit(1)

    initPos = int(sys.argv[1])
    direction = -1 if initPos < 0 else 1
    initPos = abs(initPos)

    fn = sys.argv[2] if len(sys.argv) > 2 else None
    sequence = read_sequence(fn) if fn else generate_random_sequence()

    # temp for debugging
    print('initPos:\t', initPos, '\ndirection:\t', direction, '\nfn:\t\t', fn)

    print('FCFS', fcfs(sequence, initPos))
    print('SSTF', sstf(sequence, initPos))
    print('SCAN', scan(sequence, initPos, direction))
    print('C-SCAN', cscan(sequence, initPos, direction))
    print('LOOK', look(sequence, initPos, direction))
    print('C-LOOK', clook(sequence, initPos, direction))

if __name__ == '__main__':
    main()
