# cython: boundscheck=False
# cython: wraparound=False
import numpy as np
cimport numpy as np

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

    def getkey(self, np.ndarray[np.float32_t, ndim=2] x):
        """Morton-ordered key"""
        cdef int n = x.shape[0]
        cdef np.ndarray[np.uint32_t, ndim=1] mm = self.morton_table
        cdef np.ndarray[np.uint64_t, ndim=1] kkey = np.empty(n, dtype=np.uint64)
        cdef np.ndarray[np.uint32_t, ndim=2] icorner = np.empty((n,self.ndim), dtype=np.uint32)
        cdef np.ndarray[np.float32_t, ndim=1] keyfactor = self.keyfactor.astype(np.float32)
        cdef np.ndarray[np.float32_t, ndim=1] LE = self.bbox[:,0].astype(np.float32)
        cdef np.uint64_t placeholder = self.placeholder[0]
        cdef np.uint32_t *m = &mm[0]
        cdef np.uint64_t *key = &kkey[0]
        cdef np.uint32_t ii0, ii1, ii2
        cdef np.uint64_t k
        cdef int i

        for i in range(n):
            ii0 = <unsigned int> (keyfactor[0] * (x[i,0] - LE[0]))
            ii1 = <unsigned int> (keyfactor[1] * (x[i,1] - LE[1]))
            ii2 = <unsigned int> (keyfactor[2] * (x[i,2] - LE[2]))
            k = (m[ii0 >> 16 & 0xff] | m[ii1 >> 16 & 0xff] << 1 | m[ii2 >> 16 & 0xff] << 2)
            k <<= 24
            k |= (m[ii0 >> 8 & 0xff] | m[ii1 >> 8 & 0xff] << 1 | m[ii2 >> 8 & 0xff] << 2)
            k <<= 24
            k |= m[ii0 & 0xff] | m[ii1 & 0xff] << 1 | m[ii2 & 0xff] << 2
            k |= placeholder
            key[i] = k
            icorner[i,0] = ii0
            icorner[i,1] = ii1
            icorner[i,2] = ii2
        self.icorner = icorner
        return kkey

    def key_bbox(self, key, bbox):
        """Bounding box using precomputed icorner and width (bbox preallocated)"""
        bbox[:,0] = self.bbox[:,0] + self.inv_keyfactor * self.tree[key]['icorner']
        bbox[:,1] = bbox[:,0] + self.cell_width[self.tree[key]['level']]

    def make_tree(self, x):
        """Build tree"""
        cdef np.uint32_t i0 = 0
        cdef np.uint32_t i0max = x.shape[0]
        cdef int ndim = self.ndim
        cdef int clev, cell
        self.keys = self.getkey(x)
        idx = np.argsort(self.keys)
        self.keys = self.keys[idx]
        self.icorner = self.icorner[idx]
        self.x = x[idx]
        del idx
        self.tree = {}
        while i0 < i0max:
            i1 = i0 + self.ccut
            if i1 < i0max:
                clev = keylevel(self.keys[i1-1] - self.keys[i0]) - ndim
            else:
                clev = keylevel(self.hikey - self.keys[i0])
                i1 = i0max
            clev -= clev % ndim
            cell = self.make_cell(i0, i1, clev)
            self.update_parents(cell, i0, clev)
            i0 += cell
        del self.keys
        del self.icorner
        return self.tree

    def make_cell(self, np.uint32_t i0, np.uint32_t i1, int clev):
        """make subcells within cell starting at i0, ending before i1"""
        cdef int i, n
        cdef np.uint64_t k
        cdef int nsub = self.nsub
        cdef np.ndarray[np.uint32_t, ndim=1] ii = np.empty(self.ndim, np.uint32)
        cdef np.ndarray[np.uint64_t, ndim=1] keys = self.keys
        cdef int level = self.bits_per_dim - clev/self.ndim
        cdef np.uint32_t level_mask = ~np.uint32((1 << clev/self.ndim) - 1)
        cdef np.ndarray[np.uint8_t, ndim=1] cdx = np.empty(i1-i0, dtype=np.uint8)
        cdef int cell = 0

        for i in range(i1-i0):
            cdx[i] = (keys[i0+i] >> clev) & 0x3f

        while cell < i1-i0 and cdx[cell] ^ cdx[0] < nsub:
            n = n_in_cell(cdx[cell:])
            k = keys[i0+cell] >> clev
            ii = self.icorner[i0+cell] & level_mask
            self.tree[k] = np.array((0, level, i0+cell, n, ii), dtype=self.node)
            cell += n
        return cell

    def update_parents(self, int cell, np.uint32_t i0, int clev):
        """Create/update parent cells"""
        cdef int lev, level
        cdef np.uint64_t k, key
        cdef np.ndarray[np.uint32_t, ndim=1] ii = np.empty(self.ndim, np.uint32)
        cdef np.ndarray[np.uint64_t, ndim=1] keys = self.keys
        key = keys[i0]
        for lev in range(clev+self.ndim, self.keybits+1, self.ndim):
            k = key >> lev
            if k in self.tree:
                self.tree[k]['len'] += cell
            else:
                level = self.bits_per_dim-lev/self.ndim
                ii = self.icorner[i0] & ~np.uint32((1 << lev/self.ndim) - 1)
                self.tree[k] = np.array((255, level, i0, cell, ii), dtype=self.node)

    def sphere_overlap(self, a, pos, r2):
        """Test if any part of box a is inside sphere"""
        center = 0.5 * (a[:,0] + a[:,1])
        dr = center-pos
        dr2 = np.dot(dr, dr)
        w = (a[:,1]-a[:,0])
        w2 = np.dot(w, w)
        return dr2 < w2 + r2

    def find_nbrs(self, pos, r):
        cdef int i
        cdef np.uint64_t k, cell
        cdef np.ndarray[np.uint64_t, ndim=1] nodes = np.array([8], dtype=np.uint64)
        bbox = np.empty((3,2), np.float32)
        bbox[:,0] = pos-r
        bbox[:,1] = pos+r
        r2 = r**2
        newnodes = []
        nbrlist = []
        abox = np.empty((3,2), np.float32)
        while nodes.shape[0]:
            for cell in nodes:
                for i in range(8):
                    k = cell | i
                    if k not in self.tree: continue
                    self.key_bbox(k, abox)
                    if bbox_overlap(abox, bbox): # and self.sphere_overlap(abox, pos, r2):
                        if self.tree[k]['sbits']: # no daughters implies it is terminal
                            newnodes.append(k << 3)
                        else:
                            nbrlist.append(k)
            nodes = np.array(newnodes, dtype=np.uint64)
            newnodes = []
        return nbrlist

cdef keylevel(np.uint64_t key):
    """keybits - clz(), tree root at zero"""
    cdef int lev = 0
    if key > 0xffffffffU: 
        key >>= 32
        lev += 32
    if key > 0xffffU: 
        key >>= 16
        lev += 16
    if key > 0xFFU:
        key >>= 8
        lev += 8
    if key > 0xfU:
        key >>= 4
        lev += 4
    if key > 0x3U:
        key >>= 2
        lev += 2
    if key > 0x1U:
        lev += 1
    return lev

cdef n_in_cell(np.ndarray[np.uint8_t, ndim=1] a):
    """bisection search to find count of identical vals"""
    cdef np.uint8_t val
    cdef int lo = 0
    cdef int hi = a.shape[0]
    cdef int mid = hi >> 1
    while lo < hi:    
        val = a[mid]
        if val == a[0]:
            lo = mid+1
        else: 
            hi = mid
        mid = (lo+hi) >> 1
    return hi

cdef inline bbox_overlap(a, b):
    """Test if any part of box a is inside box b"""
    return np.all(((a[:,0] >= b[:,0]) & (a[:,0]  < b[:,1])) | 
              ((a[:,1] >= b[:,0]) & (a[:,1]  < b[:,1])) |
              ((a[:,0] <= b[:,0]) & (a[:,1] >= b[:,1])))


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
