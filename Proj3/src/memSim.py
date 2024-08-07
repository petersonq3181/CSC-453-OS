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
        self.pageOrderQueue = collections.deque()
        self.lruCache = collections.OrderedDict()
    
    def load_page_fifo(self, pageNumber, content, pageTable, tlb):
        freeIdx = self.find_free()

        if freeIdx != -1:
            self.frames[freeIdx]['content'] = content
            self.frames[freeIdx]['pageNumber'] = pageNumber
            self.pageOrderQueue.append((pageNumber, freeIdx))
            pageTable.update_entry(pageNumber, freeIdx, True)
            return freeIdx
        
        oldPageNumber, oldFrameIdx = self.pageOrderQueue.popleft()
        self.frames[oldFrameIdx]['content'] = content
        self.frames[oldFrameIdx]['pageNumber'] = pageNumber
        self.pageOrderQueue.append((pageNumber, oldFrameIdx))

        pageTable.update_entry(oldPageNumber, -1, False)
        pageTable.update_entry(pageNumber, oldFrameIdx, True)
        tlb.remove_entry(oldPageNumber)

        return oldFrameIdx
    
    def load_page_lru(self, pageNumber, content, pageTable, tlb):
        if pageNumber in self.lruCache:
            frameIdx = self.lruCache.pop(pageNumber)
            self.frames[frameIdx]['content'] = content
            self.lruCache[pageNumber] = frameIdx
            return frameIdx
        
        freeIdx = self.find_free()

        if freeIdx != -1:
            self.frames[freeIdx]['content'] = content
            self.frames[freeIdx]['pageNumber'] = pageNumber
            self.lruCache[pageNumber] = freeIdx
            pageTable.update_entry(pageNumber, freeIdx, True)
            return freeIdx

        lruPageNumber, lruFrameIdx = self.lruCache.popitem(last=False)
        self.frames[lruFrameIdx]['content'] = content
        self.frames[lruFrameIdx]['pageNumber'] = pageNumber
        self.lruCache[pageNumber] = lruFrameIdx

        pageTable.update_entry(lruPageNumber, -1, False)
        tlb.remove_entry(lruPageNumber)
        pageTable.update_entry(pageNumber, lruFrameIdx, True)

        return lruFrameIdx
    
    def load_page_opt(self, pageNumber, content, futureRefs, pageTable, tlb):
        freeIdx = self.find_free()

        if freeIdx != -1:
            self.frames[freeIdx]['content'] = content
            self.frames[freeIdx]['pageNumber'] = pageNumber
            pageTable.update_entry(pageNumber, freeIdx, True)
            return freeIdx

        if futureRefs:
            pageIdx = futureRefs[-1]

            frameIdx = 0 
            for i, f in enumerate(self.frames):
                if f['pageNumber'] == pageIdx:
                    frameIdx = i
        else: 
            frameIdx = 0
            pageIdx = self.frames[frameIdx]['pageNumber']
        
        pageTable.update_entry(pageIdx, -1, False)
        tlb.remove_entry(pageIdx)
        pageTable.update_entry(pageNumber, frameIdx, True)

        return frameIdx
    
    def find_free(self):
        for index, frame in enumerate(self.frames):
            if frame['content'] is None and frame['pageNumber'] is None:
                return index
        return -1 
    
    def record_access(self, pageNumber):
        if pageNumber in self.lruCache:
            frameIndex = self.lruCache.pop(pageNumber)
            self.lruCache[pageNumber] = frameIndex
    
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
    
    binRepr = format(addr, '016b')
    
    pageNumber = int(binRepr[:8], 2)
    frameOffset = int(binRepr[-8:], 2)
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

            if not (0 < NUM_FRAMES <= PF_SIZE):
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
    refTuples = [(virtualAddr, *split_virtual(virtualAddr)) for virtualAddr in refSeq]

    # ----- init mmu structures 
    pageTable = PageTable()
    tlb = TLB()
    physMem = PhysicalMemory(NUM_FRAMES)
    bs = BackingStore(BIN_PATH)
    bs.load()

    # ----- memory access process
    n = 0
    numFaults = 0
    numTLBMisses = 0

    for virtualAddr, pageNumber, frameOffset in refTuples: 
        n += 1
        
        if PRA.lower() == 'opt':
            futureSeq = [pn for _, pn, _ in refTuples[n:]]

        # check TLB
        frameNumber = -1 
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
                    frameNumber = physMem.load_page_lru(pageNumber, page, pageTable, tlb)
                elif PRA.lower() == 'opt': 
                    frameNumber = physMem.load_page_opt(pageNumber, page, futureSeq, pageTable, tlb)

            tlb.add_entry(pageNumber, frameNumber)

        if PRA.lower() == 'lru': 
            physMem.record_access(pageNumber)

        print('{}, {}, {}, {}'.format(virtualAddr, value, frameNumber, format_byte_arr(page)))
    print('Number of Translated Addresses = {}'.format(n))
    print('Page Faults = {}'.format(numFaults))
    print('Page Fault Rate = {:.3f}'.format(numFaults / n))
    print('TLB Hits = {}'.format(n - numTLBMisses))
    print('TLB Misses = {}'.format(numTLBMisses))
    print('TLB Hit Rate = {:.3f}\n'.format((n - numTLBMisses) / n))

if __name__ == '__main__':
    main()
