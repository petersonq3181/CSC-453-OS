import sys

NUM_PAGES = 256 
NUM_FRAMES = 256 
PF_SIZE = 256 
TLB_SIZE = 16 
PRA = 'fifo'
BIN_PATH = 'BACKING_STORE.bin'

class PageTable:
    def __init__(self):
        # pair represents: (frame number, loaded bit)
        # index of entries represents the page number 
        self.entries = [(-1, False) for _ in range(NUM_PAGES)]

    def idx(self, pageNumber):
        return self.entries[pageNumber] if self.entries[pageNumber] is not None else -1
    
    def update_entry(self, pageNumber, frameNumber, loaded):
        self.entries[pageNumber] = (frameNumber, loaded)

    def print_state(self): 
        print('\n----- Page Table:')
        for page, (frame, loaded) in enumerate(self.entries):
            print('Page:', page, 'Frame:', frame, 'Loaded:', loaded)
        print('\n')

class TLB: 
    # TODO refactor so stuff is put and taken out in FIFO order 
    def __init__(self):
        # pair represents: (page number, frame number)
        self.entries = [(None, None) for _ in range(TLB_SIZE)]

    def idx(self, pageNumber):
        for entry in self.entries:
            if entry[0] == pageNumber:
                return entry[1]
        return -1
    
    def print_state(self): 
        print('\n----- TLB:')
        for (page, frame) in self.entries:
            print('Page:', page, 'Frame:', frame)
        print('\n')
    
class PhysicalMemory:
    def __init__(self, numFrames):
        self.frames = [{'content': None, 'pageNumber': None} for _ in range(numFrames)]
        self.numFrames = numFrames

    def load_page(self, pageNumber, content):
        for frame in self.frames:
            if frame['content'] is None:
                frame['content'] = content
                frame['pageNumber'] = pageNumber
                return True
        return False
    
    # TODO refactor; might need to use a free-frames queue instead 
    def find_free(self):
        for index, frame in enumerate(self.frames):
            if frame['content'] is None and frame['pageNumber'] is None:
                return index
        return -1 
    
    def print_state(self): 
        print('\n----- Physical Memory:')
        for index, frame in enumerate(self.frames):
            content = frame['content']
            pageNumber = frame['pageNumber']
            print('Frame', index, ', Page Number:', pageNumber, ', Content:', content, )
        print('\n')
    
class BackingStore:
    def __init__(self, filePath):
        self.filePath = filePath
        self.data = None
    
    def load(self):
        with open(self.filePath, 'rb') as f:
            self.data = bytearray(f.read())
    
    def get_page(self, pageNumber):
        if self.data is None:
            raise ValueError('Data not loaded; call load() first')
        
        if not (0 <= pageNumber < 256):
            raise ValueError('Page number must be between 0 and 255')
        
        start = pageNumber * PF_SIZE
        end = start + PF_SIZE
        return self.data[start:end]
    
def get_reference_seq(rf):
    refSeq = [] 
    try:
        with open(rf, 'r') as file:
            for line in file:
                refSeq.append(int(line.strip()))
    except FileNotFoundError:
        print(f'Error: The file {rf} was not found')
        sys.exit(1)
    except ValueError as e:
        print(f'Error: Problem converting a line to an integer: {e}')
        sys.exit(1)

    return refSeq

def split_virtual(addr):
    if not 0 <= addr <= 65535:
        raise ValueError('addr must be a 16-bit unsigned integer')
    
    binary_representation = format(addr, '016b')
    
    pageNumber = int(binary_representation[:8], 2)
    frameOffset = int(binary_representation[-8:], 2)
    
    return pageNumber, frameOffset

def format_byte_arr(arr):
    hex = arr.hex().upper()
    out = ''.join(hex[i:i + 4] for i in range(0, len(hex), 4))
    return out





def main():

    # ----- input parsing 
    if len(sys.argv) < 2:
        print('Error: Must provide at least one argument')
        sys.exit(1)

    global NUM_FRAMES, PRA

    rf = sys.argv[1]
    
    if len(sys.argv) > 2:
        try:
            NUM_FRAMES = int(sys.argv[2])

            if not (0 <= NUM_FRAMES < PF_SIZE):
                raise ValueError
        except ValueError:
            print('Error: NUM_FRAMES must be an integer in a valid range')
            sys.exit(1)

    if len(sys.argv) > 3:
        PRA = sys.argv[3]
        if PRA not in ['fifo', 'lru', 'opt']:
            print('Error: PRA is not a valid string') 
            sys.exit(1)

    print('got inputs: \n\t file: %s \n\t frames: %d \n\t PRA %s' % (rf, NUM_FRAMES, PRA))

    # ----- read in reference sequence 
    refSeq = get_reference_seq(rf)
    print('reference sequence: ', refSeq)

    # ----- instantiate hardware / mmu structures 
    pageTable = PageTable()
    tlb = TLB()
    physMem = PhysicalMemory(NUM_FRAMES)
    bs = BackingStore(BIN_PATH)
    bs.load()

    # ----- memory access process with TLB
    numFaults = 0
    numTLBMisses = 0
    
    
    # TODO (for now; change later)
    # virtualAddr = refSeq[0]
    for virtualAddr in refSeq: 

        pageNumber, frameOffset = split_virtual(virtualAddr)

        frameNumber = -1 

        # check TLB
        frameNumber = tlb.idx(pageNumber)
        if frameNumber == -1: 
            numTLBMisses += 1

            # check page table 
            frameNumber, loadedBit = pageTable.idx(pageNumber)
            if frameNumber == -1: 
                numFaults += 1 

                # --- PAGE FAULT: case where page entry not found in TLB nor page table 
                # bring in missing page (TODO for now assuming no swapping required)
                frameNumber = physMem.find_free()
                if frameNumber == -1:
                    pass 

                # correct page table and other structures 
                # load from backing store into frame 
                page = bs.get_page(pageNumber)
                physMem.load_page(pageNumber, page)
                pageTable.update_entry(pageNumber, frameNumber, True)

        print(virtualAddr, '?', frameNumber, format_byte_arr(page))

    print(numFaults)
    print(numTLBMisses)



if __name__ == '__main__':
    main()
