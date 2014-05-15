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
        self.domain_width += 4.0 * np.finfo(bbox.dtype).eps * self.domain_width
        self.bbox[:,0] -= 2.0 * np.finfo(bbox.dtype).eps * self.domain_width
        self.bbox[:,1] += 2.0 * np.finfo(bbox.dtype).eps * self.domain_width
        self.cell_width = np.outer(self.domain_width, 2.0 ** -np.arange(0, self.bits_per_dim)).T
        self.keyfactor = (np.ones(1, dtype=self.keytype) << self.bits_per_dim) / self.domain_width
        self.inv_keyfactor = 1.0 / self.keyfactor
        self.lokey = self.placeholder
        self.hikey = self.placeholder | self.placeholder-1
        self.node = np.dtype([('sbits', np.uint8), # sub-bits, True if daughter exists
                              ('level', np.uint8),
                              ('base', np.uint32),
                              ('len', np.uint32),
                              ('icorner', np.uint32, (3))])
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
        self.icorner = ii.T
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

    def key_bbox(self, key, bbox):
        """Bounding box using precomputed icorner and width (bbox preallocated)"""
        bbox[:,0] = self.bbox[:,0] + self.inv_keyfactor * self.tree[key]['icorner']
        bbox[:,1] = bbox[:,0] + self.cell_width[self.tree[key]['level']]

    def key_bbox2(self, key):
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

    def keybits(self, x):
        """Print bitstring"""
        return [bin(k)[2:] for k in self.getkey(x)]

    def make_tree(self, x):
        """Build tree"""
        self.keys = self.getkey(x)
        idx = np.argsort(self.keys)
        self.keys = self.keys[idx]
        self.icorner = self.icorner[idx]
        self.x = x[idx]
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
                clev = keylevel(self.keys[i1-1] - self.keys[i0]) - self.ndim
            else:
                clev = keylevel(self.hikey - self.keys[i0])
                i1 = i0max
            clev -= clev % self.ndim
            while (self.keys[i0:i0+1] >> clev)[0] in self.tree:
                clev -= self.ndim
            cell = self.make_cell(i0, i1, clev)
            self.update_parents(cell, i0, clev)
            i0 += cell
        del self.keys
        del self.icorner
        print 'tree cell_minn: %d cell_maxn: %d avg: %.3f' % (self.cell_minn, self.cell_maxn, len(self.x)/self.ncells)
        return self.tree

    def make_cell(self, i0, i1, clev):
        """make subcells within cell starting at i0, ending before i1"""
        level = self.bits_per_dim - clev/self.ndim
        level_mask = ~np.uint32((1 << clev/self.ndim) - 1)
        cdx = cellidx(self.keys[i0:i1], clev)
        cell = 0
        while cell < i1-i0 and cdx[cell] ^ cdx[0] < self.nsub:
            n = n_in_cell(cdx[cell:])
            k = (self.keys[i0+cell:i0+cell+1] >> clev)[0]
            ii = self.icorner[i0+cell] & level_mask
            self.tree[k] = np.array((0, level, i0+cell, n, ii), dtype=self.node)
            if n > self.cell_maxn: self.cell_maxn = n # keep some statistics
            if n < self.cell_minn: self.cell_minn = n
            self.ncells += 1
            cell += n
        return cell

    def update_parents(self, cell, i0, clev):
        """Create/update parent cells"""
        for lev in range(clev+self.ndim, self.keybits+1, self.ndim):
            k = (self.keys[i0:i0+1] >> lev)[0]
            if k in self.tree:
                self.tree[k]['len'] += cell
            else:
                level = self.bits_per_dim-lev/self.ndim
                ii = self.icorner[i0] & ~np.uint32((1 << lev/self.ndim) - 1)
                self.tree[k] = np.array((255, level, i0, cell, ii), dtype=self.node)

    def bbox_overlap(self, a, b):
        """Test if any part of box a is inside box b"""
        return np.all(((a[:,0] >= b[:,0]) & (a[:,0]  < b[:,1])) | 
                      ((a[:,1] >= b[:,0]) & (a[:,1]  < b[:,1])) |
                      ((a[:,0] <= b[:,0]) & (a[:,1] >= b[:,1])))

    def sphere_overlap(self, a, pos, r2):
        """Test if any part of box a is inside sphere"""
        center = 0.5 * (a[:,0] + a[:,1])
        dr = center-pos
        dr2 = np.dot(dr, dr)
        w = (a[:,1]-a[:,0])
        w2 = np.dot(w, w)
        return dr2 < w2 + r2

    def find_nbrs(self, pos, r):
        bbox = np.empty((3,2), np.float32)
        bbox[:,0] = pos-r
        bbox[:,1] = pos+r
        r2 = r**2
        nodes = np.array([8], dtype=self.keytype)
        newnodes = []
        nbrlist = []
        abox = np.empty((3,2), np.float32)
        while nodes.shape[0]:
            for cell in nodes:
                for i in range(self.nsub):
                    k = (np.array([cell]) | i)[0]
                    if k not in self.tree: continue
                    self.key_bbox(k, abox)
                    if self.bbox_overlap(abox, bbox): # and self.sphere_overlap(abox, pos, r2):
                        if self.tree[k]['sbits']: # no daughters implies it is terminal
                            newnodes.append((np.array([k]) << self.ndim)[0])
                        else:
                            nbrlist.append(k)
            nodes = np.array(newnodes, dtype=self.keytype)
            newnodes = []
        return nbrlist

    def check_bbox(self):
        nodes = np.array([8], dtype=self.keytype)
        newnodes = []
        abox = np.empty((3,2), np.float32)
        while nodes.shape[0]:
            for cell in nodes:
                for i in range(self.nsub):
                    k = (np.array([cell]) | i)[0]
                    if k not in self.tree: continue
                    if self.tree[k]['sbits']: # no daughters implies it is terminal
                        newnodes.append((np.array([k]) << self.ndim)[0])
                    else:
                        self.key_bbox(k, abox)
                        node = self.tree[k]
                        for p in self.x[node['base']:node['base']+node['len']]:
                            if p[0] < abox[0,0] or p[0] > abox[0,1] or \
                               p[1] < abox[1,0] or p[1] > abox[1,1] or \
                               p[2] < abox[2,0] or p[2] > abox[2,1]:
                                print 'Bad bbox', k, node, p, abox
                                import pdb; pdb.set_trace()
            nodes = np.array(newnodes, dtype=self.keytype)
            newnodes = []

    def check_find(self, start=0):
        for i,x in enumerate(self.x[start:]):
            if (i % 100) == 0: print i
            nbrs = self.find_nbrs(x, self.domain_width * 1e-7)
            ok = False
            for k in nbrs:
                node = self.tree[k]
                for p in self.x[node['base']:node['base']+node['len']]:
                    if np.array_equal(p, x):
                        ok = True
                        break
            if not ok:
                print 'ERROR, could not find x[%d]' % i
                import pdb; pdb.set_trace()

def keylevel(key):
    """keybits - clz(), tree root at zero"""
    lev = 0
    # numpy does not currently support shift on 64-bit scalar ints
    key = np.array([key])
    if key > np.uint64(0xffffffff): 
        key >>= 32
        lev += 32
    if key > np.uint64(0xffff): 
        key >>= 16
        lev += 16
    if key > np.uint64(0xff): 
        key >>= 8
        lev += 8
    if key > np.uint64(0xf): 
        key >>= 4
        lev += 4
    if key > np.uint64(0x3): 
        key >>= 2
        lev += 2
    if key > np.uint64(0x1): 
        lev += 1
    return lev

def cellidx(keys, clev):
    """subcell indices"""
    return np.uint8((keys >> clev) & 0x3f)

def n_in_cell(a):
    """bisection search to find count of identical vals"""
    lo = 0
    hi = len(a)
    mid = hi >> 1
    while lo < hi:    
        val = a[mid]
        if val == a[0]:
            lo = mid+1
        else: 
            hi = mid
        mid = (lo+hi) >> 1
    return hi


if __name__ == "__main__":
    from time import *

    npart = 1e7
    ot = HOT()

    np.random.seed(0)
    x = np.random.rand(npart,ot.ndim).astype(np.float32)
    
    t0 = time()
    tree = ot.make_tree(x)
    x = ot.x                    # Morton ordered
    print tree[1]['len'], 'particles'
    print 'build time:', time()-t0
    
    r = 0.05
    pos = np.array([0.6, 0.6, 0.6], dtype=np.float32)

    t0 = time()
    nbrs = ot.find_nbrs(pos, r)
    sum = 0
    for k in nbrs:
        sum += tree[k]['len']

    print 'found %d overlaps, %d particles' % (len(nbrs), sum)
    print 'find nbrs time:', time()-t0

    t0 = time()
    snbrs = 0
    for k in nbrs:
        xx = x[tree[k]['base']:tree[k]['base']+tree[k]['len']]
        dr = xx-pos
        dr2 = np.sum(dr*dr, axis=1)
        snbrs += np.sum(dr2 <= r**2)

    print '%d particles within search radius' % snbrs
    print 'search time:', time()-t0

    t0 = time()
    dr = x-pos
    dr2 = np.sum(dr*dr,axis=1)
    snbrs = np.sum(dr2 <= r**2)

    print '%d particles within search radius' % snbrs
    print 'search time:', time()-t0
