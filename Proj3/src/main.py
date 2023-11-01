import sys

NUM_PAGES = 256 
NUM_FRAMES = 256 
PF_SIZE = 256 
TLB_SIZE = 16 
PRA = 'fifo'
NUM_FAULTS = 0 

class PageTable:
    def __init__(self):
        # pair represents: (frame number, loaded bit)
        # index of entries represents the page number 
        self.entries = [(-1, False) for _ in range(NUM_PAGES)]

    def idx(self, pageNumber):
        return self.entries[pageNumber] if self.entries[pageNumber] is not None else -1


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

def split_virtual(addr):
    if not 0 <= addr <= 65535:
        raise ValueError("addr must be a 16-bit unsigned integer")
    
    binary_representation = format(addr, '016b')
    
    pageNumber = int(binary_representation[:8], 2)
    frameOffset = int(binary_representation[-8:], 2)
    
    return pageNumber, frameOffset



def main():

    # ----- program input parsing 
    if len(sys.argv) < 2:
        print('Error: Must provide at least one argument')
        sys.exit(1)

    global NUM_FRAMES, PRA, NUM_FAULTS

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

    # ---- instantiate main hardware / mmu structures 
    pageTable = PageTable()
    tlb = TLB()
    physMem = PhysicalMemory(NUM_FRAMES)

    # ----- 
    try:
        with open(rf, 'r') as file:
            for line in file:
                virtualAddr = int(line.strip())
                
                pageNumber, frameOffset = split_virtual(virtualAddr)

                frameNumber = -1 

                # check TLB
                frameNumber = tlb.idx(pageNumber)
                if frameNumber == -1: 
                    # check page table 
                    frameNumber, loadedBit = pageTable.idx(pageNumber)
                    if frameNumber == -1: 

                        # --- PAGE FAULT: case where page entry not found in TLB nor page table 
                        # bring in missing page (TODO for now assuming no swapping required)
                        freeIdx = physMem.find_free()
                        if freeIdx == -1:
                            # TODO swapping 
                            pass 



                        # correct page table and other structures 







                # testing this for one addr for now 
                return 
    except FileNotFoundError:
        print(f'Error: The file {rf} was not found')
        sys.exit(1)




if __name__ == "__main__":
    main()
