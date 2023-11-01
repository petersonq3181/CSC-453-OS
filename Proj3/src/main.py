import sys

NUM_PAGES = 256 
NUM_FRAMES = 256 
PF_SIZE = 256 
TLB_SIZE = 16 
PRA = 'fifo'

class PageTable:
    def __init__(self):
        self.entries = [{'f': None, 'loaded': False} for _ in range(NUM_PAGES)]

class TLB: 
    def __init__(self):
        self.entries = [{'p': None, 'f': None} for _ in range(TLB_SIZE)]



def main():
    if len(sys.argv) < 2:
        print('Error: Must provide at least one argument')
        sys.exit(1)

    global NUM_FRAMES, PRA

    rf = sys.argv[1]
    
    if len(sys.argv) > 2:
        try:
            NUM_FRAMES = int(sys.argv[2])
        except ValueError:
            print('Error: NUM_FRAMES must be an integer')
            sys.exit(1)

    if len(sys.argv) > 3:
        PRA = sys.argv[3]

    print('got inputs: \n\t file: %s \n\t frames: %d \n\t PRA %s' % (rf, NUM_FRAMES, PRA))


if __name__ == "__main__":
    main()
