import sys
import collections

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
    def __init__(self):
        # pair represents: (page number, frame number)
        self.entries = [(None, None) for _ in range(TLB_SIZE)]
        self.fifoQueue = collections.deque()

    def idx(self, pageNumber):
        for page, frame in self.entries:
            if page == pageNumber:
                return frame
        return -1

    def add_entry(self, pageNumber, frameNumber):
        for index, (page, frame) in enumerate(self.entries):
            if page is None:
                self.entries[index] = (pageNumber, frameNumber)
                self.fifoQueue.append(index)
                return
        
        oldestIdx = self.fifoQueue.popleft()
        self.entries[oldestIdx] = (pageNumber, frameNumber)
        self.fifoQueue.append(oldestIdx)
    
    def remove_entry(self, pageNumber):
        for index, (page, frame) in enumerate(self.entries):
            if page == pageNumber:
                self.entries[index] = (None, None)
                self.fifoQueue.remove(index)
                break
    
    def print_state(self): 
        print('\n----- TLB:')
        for (page, frame) in self.entries:
            print('Page:', page, 'Frame:', frame)
        print('\n')
    
class PhysicalMemory:
    def __init__(self, numFrames):
        self.frames = [{'content': None, 'pageNumber': None} for _ in range(numFrames)]
        self.numFrames = numFrames
        self.pageOrderQueue = collections.deque()  # FIFO queue to store the order of loaded pages

    # original load page (TODO might delete later)
    def load_page(self, pageNumber, content):
        for frame in self.frames:
            if frame['content'] is None:
                frame['content'] = content
                frame['pageNumber'] = pageNumber
                return True
        return False
    
    def load_page_fifo(self, pageNumber, content, pageTable, tlb):
        freeFrameIndex = self.find_free()

        # if there is a free frame 
        if freeFrameIndex != -1:
            self.frames[freeFrameIndex]['content'] = content
            self.frames[freeFrameIndex]['pageNumber'] = pageNumber
            self.pageOrderQueue.append((pageNumber, freeFrameIndex))  # Add the page to the FIFO queue
            pageTable.update_entry(pageNumber, freeFrameIndex, True)
            return freeFrameIndex
        
        # if no free frame is available perform FIFO page replacement
        oldPageNumber, oldFrameIndex = self.pageOrderQueue.popleft()
        self.frames[oldFrameIndex]['content'] = content
        self.frames[oldFrameIndex]['pageNumber'] = pageNumber
        self.pageOrderQueue.append((pageNumber, oldFrameIndex))

        # update page table and TLB
        pageTable.update_entry(oldPageNumber, -1, False)
        pageTable.update_entry(pageNumber, oldFrameIndex, True)
        tlb.remove_entry(oldPageNumber)
        return freeFrameIndex
    
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

    # ----- read in reference sequence 
    refSeq = get_reference_seq(rf)

    # ----- init mmu structures 
    pageTable = PageTable()
    tlb = TLB()
    physMem = PhysicalMemory(NUM_FRAMES)
    bs = BackingStore(BIN_PATH)
    bs.load()

    # ----- memory access process with TLB
    n = 0
    numFaults = 0
    numTLBMisses = 0
    
    for virtualAddr in refSeq: 
        n += 1
        pageNumber, frameOffset = split_virtual(virtualAddr)

        frameNumber = -1 

        # check TLB
        frameNumber = tlb.idx(pageNumber)
        if frameNumber == -1: 
            numTLBMisses += 1

            # check page table, if not found then page fault 
            frameNumber, loadedBit = pageTable.idx(pageNumber)
            if frameNumber == -1: 
                numFaults += 1 

                page = bs.get_page(pageNumber)
                
                value = int.from_bytes(page[frameOffset:frameOffset+1], 'little', signed=True)

                if PRA.lower() == 'fifo': 
                    frameNumber = physMem.load_page_fifo(pageNumber, page, pageTable, tlb)
                elif PRA.lower() == 'lru': 
                    pass 

            tlb.add_entry(pageNumber, frameNumber)

        print('{}, {}, {}, {}'.format(virtualAddr, value, frameNumber, format_byte_arr(page)))
    print('Number of Translated Addresses = {}'.format(n))
    print('Page Faults = {}'.format(numFaults))
    print('Page Fault Rate = {:.3f}'.format(numFaults / n))
    print('TLB Hits = {}'.format(n - numTLBMisses))
    print('TLB Misses = {}'.format(numTLBMisses))
    print('TLB Hit Rate = {:.3f}\n'.format((n - numTLBMisses) / n))

if __name__ == '__main__':
    main()
