import numpy as np

class HOT(object):
    """
    Hashed tree
    """

    def __init__(self,
                 bbox=np.array([[0.0]*3, [1.0]*3]).T):
        
        self.bbox = bbox
        self.ndim = np.shape(self.bbox)[0]
        self.nsub = 1 << self.ndim
        self.keytype = np.uint64
        self.placeholder = np.ones(1, dtype=self.keytype) # uint128 support would be nice
        self.bits_per_dim = self.placeholder.itemsize * 8 / self.ndim
        self.keybits = self.ndim * self.bits_per_dim
        self.ccut = 512         # max nodes in next-to-leaf cell
        self.placeholder <<= self.keybits
        self.domain_width = bbox[:,1] - bbox[:,0]
        self.domain_width *= 1.0 + 4.0 * np.finfo(bbox.dtype).eps
        self.keyfactor = (np.ones(1, dtype=self.keytype) << self.bits_per_dim) / self.domain_width
        self.lokey = self.placeholder
        self.hikey = self.placeholder | self.placeholder-1
        self.node = np.dtype([('sbits', np.uint8), # sub-bits, True if daughter exists
                              ('base', np.uint32),
                              ('len', np.uint32),
                              ('bbox', np.float32, (self.ndim,2))])
        self.morton_table = np.zeros((256), dtype=np.uint32)
        for i in range(256):
            self.morton_table[i] = i&1 | (i>>1&1)<<3 | (i>>2&1)<<6 | (i>>3&1)<<9 | (i>>4&1)<<12 | (i>>5&1)<<15 | (i>>6&1)<<18 | (i>>7&1)<<21
        self.inv_morton_table = np.zeros((64), dtype=np.uint8)
        for i in range(64):
            self.inv_morton_table[i] = i&1 | (i>>2)&2 | (i<<1)&4 | (i>>1)&8 | (i<<2)&16 | i&32

    def getkey(self, x):
        """Morton-ordered key"""
        ii = self.keyfactor * (x - self.bbox[:,0])
        ii = ii.astype(np.uint32, copy=False).T
        m = self.morton_table
        n = 1
        if x.ndim > 1: n = ii.shape[1]
        key = np.zeros(n, self.placeholder.dtype)
        key |= (m[ii[0] >> 16 & 0xff] | m[ii[1] >> 16 & 0xff] << 1 | m[ii[2] >> 16 & 0xff] << 2)
        key <<= 24
        key |= (m[ii[0] >> 8 & 0xff] | m[ii[1] >> 8 & 0xff] << 1 | m[ii[2] >> 8 & 0xff] << 2)
        key <<= 24
        key |= m[ii[0] & 0xff] | m[ii[1] & 0xff] << 1 | m[ii[2] & 0xff] << 2
        key |= self.placeholder[0]
        return key

    def inv_getkey(self, key_):
        corner = [0] * 3
        width = 1
        key = key_.copy()
        while key > 1:
            if key & 1: corner[0] |= width
            if key & 2: corner[1] |= width
            if key & 4: corner[2] |= width
            key >>= self.ndim
            width <<= 1
        return corner, width

    def key_bbox(self, key):
        """Bounding box from Morton key"""
        icorner, iwidth = self.inv_getkey(key)
        bbox = np.empty((3,2), np.float32)
        width = self.domain_width/iwidth
        bbox[:,0] = self.bbox[:,0] + width * icorner
        bbox[:,1] = bbox[:,0] + width
        return bbox
    
    def sort(self, x):
        """Sort in Morton order"""
        return x[np.argsort(self.getkey(x))]

    def popcnt(self, key):
        """Number of 1 bits in key""" # Hamming weight
        return bin(key).count('1')

    def level(self, key):
        """keybits - clz(), tree root at zero"""
        lev = 0
        # numpy does not currently support shift on 64-bit scalar ints
        if key > self.keytype(0xffffffff): 
            key /= self.keytype(2**32)
            lev += 32
        if key > self.keytype(0xffff): 
            key /= self.keytype(2**16)
            lev += 16
        if key > self.keytype(0xff): 
            key /= self.keytype(2**8)
            lev += 8
        if key > self.keytype(0xf): 
            key /= self.keytype(2**4)
            lev += 4
        if key > self.keytype(0x3): 
            key /= self.keytype(2**2)
            lev += 2
        if key > self.keytype(0x1): 
            lev += 1
        return lev

    def keybits(self, x):
        """Print bitstring"""
        return [bin(k)[2:] for k in self.getkey(x)]

    def make_tree(self, x):
        """Build tree"""
        keys = self.getkey(x)
        idx = np.argsort(keys)
        x = x[idx]
        self.x = x
        self.keys = keys[idx]
        del keys
        del idx
        i0 = 0
        i0max = len(x)
        self.tree = {}
        self.cell_maxn = 0
        self.cell_minn = i0max
        self.ncells = 0
        while i0 < i0max:
            i1 = i0 + self.ccut
            if i1 < i0max:
                clev = self.level(self.keys[i1-1] - self.keys[i0]) - self.ndim
            else:
                clev = self.level(self.hikey - self.keys[i0])
                i1 = i0max
            clev -= clev % self.ndim
            cell = self.make_cell(i0, i1, clev)
            self.update_parents(cell, i0, clev)
            i0 += cell
        return self.tree

    def cellidx(self, i0, i1, clev):
        """subcell indices"""
        return np.uint8((self.keys[i0:i1] >> clev) & 0x3f)

    def make_cell(self, i0, i1, clev):
        """make subcells within cell starting at i0, ending before i1"""
        cell = 0
        cdx = self.cellidx(i0, i1, clev)
        while cell < i1-i0 and cdx[cell] ^ cdx[0] < self.nsub:
            n = 0
            while cell+n < i1-i0 and cdx[cell] == cdx[cell+n]: n += 1
            k = self.keys[i0+cell:i0+cell+1] >> clev
            self.tree[k[0]] = np.array((0, i0+cell, n, self.key_bbox(k)), dtype=self.node)
            cell += n
            if n > self.cell_maxn: self.cell_maxn = n # keep some statistics
            if n < self.cell_minn: self.cell_minn = n
            self.ncells += 1
        return cell

    def update_parents(self, cell, i0, clev):
        """Create/update parent cells"""
        for lev in range(clev+self.ndim, self.keybits+1, self.ndim):
            k = self.keys[i0:i0+1] >> lev
            if k[0] in self.tree:
                self.tree[k[0]]['len'] += cell
            else:
                self.tree[k[0]] = np.array((255, i0, cell, self.key_bbox(k)), dtype=self.node)

    def bbox_overlap(self, a, b):
        """Test if any part of box a is inside box b"""
        return (
            ((a[0,0] >= b[0,0] and a[0,0] < b[0,1]) or (a[0,1] >= b[0,0] and a[0,1] < b[0,1])) and
            ((a[1,0] >= b[1,0] and a[1,0] < b[1,1]) or (a[1,1] >= b[1,0] and a[1,1] < b[1,1])) and
            ((a[2,0] >= b[2,0] and a[2,0] < b[2,1]) or (a[2,1] >= b[2,0] and a[2,1] < b[2,1])))

    def find_nbrs(self, bbox):
        nodes = np.ones(1, dtype=self.keytype)
        newnodes = []
        nbrlist = []
        while len(nodes):
            for cell in nodes:
                seq = np.arange(self.nsub)
                sbits = (1 << seq) & tree[cell]['sbits']
                subcell = (np.array([cell]) << self.ndim) | seq.astype(self.keytype)
                for i,k in enumerate(subcell):
                    if sbits[i] and self.bbox_overlap(tree[k]['bbox'], bbox):
                        if tree[k]['sbits']: # no daughters implies it is terminal
                            newnodes.append(k)
                        else:
                            nbrlist.append(k)
            nodes = np.array(newnodes)
            newnodes = []
        return nbrlist

if __name__ == "__main__":
    from time import *

    npart = 1e6
    ot = HOT()

    np.random.seed(0)
    x = np.random.rand(npart,ot.ndim).astype(np.float32)
    
    t0 = time()
    tree = ot.make_tree(x)
    x = ot.x                    # Morton ordered
    print tree[1]['len'], ot.cell_minn, ot.cell_maxn, len(x)/np.float32(ot.ncells)
    print 'build time:', time()-t0
    
    rmax = 0.05
    rmax2 = rmax**2
    bbox = np.array([[0.5-rmax]*3, [0.5+rmax]*3], dtype=np.float32).T

    t0 = time()
    nbrs = ot.find_nbrs(bbox)
    sum = 0
    for k in nbrs:
        sum += tree[k]['len']

    print 'found %d overlaps, %d particles' % (len(nbrs), sum)
    print 'search time:', time()-t0

    r0 = np.array([0.5, 0.5, 0.5], dtype=np.float32)
    snbrs = 0
    for k in nbrs:
        xx = x[tree[k]['base']:tree[k]['base']+tree[k]['len']]
        dr = xx-r0
        r2 = np.sum(dr*dr,axis=1)
        snbrs += len(r2[r2<rmax2])

    print '%d particles within search radius' % snbrs
