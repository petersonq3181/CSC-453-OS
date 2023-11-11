import sys
import random

NUM_CYLS = 5000
NUM_RAND_REQS = 100

def read_seq(fn):
    seq = [] 
    try:
        with open(fn, 'r') as file:
            for line in file:
                seq.append(int(line.strip()))
    except FileNotFoundError:
        print(f'Error: The file {fn} was not found')
        sys.exit(1)
    except ValueError as e:
        print(f'Error: Problem converting a line to an integer: {e}')
        sys.exit(1)

    return seq

def gen_rand_seq():
    seq = [random.randint(0, NUM_CYLS - 1) for _ in range(NUM_RAND_REQS)]
    return seq

def fcfs(seq, pos):
    dist = 0 
    for ele in seq:
        dist += abs(pos - ele)
        pos = ele
    return dist 

def sstf(seq, pos):
    dist = 0
    sequence = seq.copy()

    while sequence:
        nxt = min(sequence, key=lambda i: abs(i - pos))
        dist += abs(nxt - pos)
        pos = nxt 
        sequence.remove(nxt) 
    return dist

def scan(seq, pos, direction):
    dist = 0
    endOfDisk = NUM_CYLS - 1 

    lowerReqs = [req for req in seq if req < pos]
    lowerReqs.sort(reverse=True) 

    upperReqs = [req for req in seq if req >= pos]
    upperReqs.sort() 

    if direction <= 0: 
        for req in lowerReqs:
            dist += abs(req - pos)
            pos = req

        if upperReqs:
            dist += abs(pos) 
            pos = 0

        for req in upperReqs:
            dist += abs(req - pos)
            pos = req
    else:  
        for req in upperReqs:
            dist += abs(req - pos)
            pos = req

        if lowerReqs:
            dist += abs(endOfDisk - pos)
            pos = endOfDisk

        for req in lowerReqs:
            dist += abs(req - pos)
            pos = req
    return dist

def cscan(seq, pos, direction):
    dist = 0
    endOfDisk = NUM_CYLS - 1

    lowerReqs = [req for req in seq if req < pos]
    lowerReqs.sort(reverse=True)

    upperReqs = [req for req in seq if req >= pos]
    upperReqs.sort()

    if direction > 0: 
        if upperReqs:
            dist += abs(pos - endOfDisk)

        dist += endOfDisk

        if lowerReqs:
            dist += max(lowerReqs)
    else: 
        if lowerReqs:
            dist += pos 

        dist += endOfDisk

        if upperReqs: 
            dist += min(upperReqs)
    return dist

def look(seq, pos, direction):
    dist = 0

    lowerReqs = [req for req in seq if req < pos]
    lowerReqs.sort(reverse=True)

    upperReqs = [req for req in seq if req >= pos]
    upperReqs.sort()

    if direction <= 0:
        for req in lowerReqs:
            dist += abs(req - pos)
            pos = req

        if upperReqs:
            dist += abs(pos - upperReqs[0])
            pos = upperReqs[0]

        for req in upperReqs:
            dist += abs(req - pos)
            pos = req
    else:
        for req in upperReqs:
            dist += abs(req - pos)
            pos = req

        if lowerReqs:
            dist += abs(pos - lowerReqs[0])
            pos = lowerReqs[0]

        for req in lowerReqs:
            dist += abs(req - pos)
            pos = req
    return dist

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
    sequence = read_seq(fn) if fn else gen_rand_seq()

    # temp for debugging
    print('initPos:\t', initPos, '\ndirection:\t', direction, '\nfn:\t\t', fn)
    print('sequence: ', sequence)
    print()

    print('FCFS', fcfs(sequence, initPos))
    print('SSTF', sstf(sequence, initPos))
    print('SCAN', scan(sequence, initPos, direction))
    print('C-SCAN', cscan(sequence, initPos, direction))
    print('LOOK', look(sequence, initPos, direction))
    print('C-LOOK', clook(sequence, initPos, direction))

if __name__ == '__main__':
    main()
