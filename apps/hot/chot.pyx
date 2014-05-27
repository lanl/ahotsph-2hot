# cython: profile=False
# cython: boundscheck=False
# cython: wraparound=False
import numpy as np
cimport numpy as np

class HOT(object):
    """
    Hashed tree
    """

    def __init__(self,
                 bbox=np.array([[0.0]*3, [1.0]*3]).T,
                 ccut=512, save_idx=False):
        
        self.bbox = bbox
        self.ndim = np.shape(self.bbox)[0]
        self.nsub = 1 << self.ndim
        self.keytype = np.uint64
        self.placeholder = np.ones(1, dtype=self.keytype) # uint128 support would be nice
        self.bits_per_dim = self.placeholder.itemsize * 8 / self.ndim
        self.keybits = self.ndim * self.bits_per_dim
        self.ccut = ccut        # max nodes in next-to-leaf cell
        self.placeholder <<= self.keybits
        self.save_idx = save_idx   # Save argsort of keys (high memory cost)
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
                              ('bbox', np.float32, (3,2))])
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

    def inv_getkey(self, np.uint64_t key):
        cdef np.ndarray[np.uint32_t, ndim=1] corner = np.zeros(self.ndim, np.uint32)
        cdef int width = 1
        cdef int ndim = self.ndim
        while key > 1:
            if key & 1: corner[0] |= width
            if key & 2: corner[1] |= width
            if key & 4: corner[2] |= width
            key >>= ndim
            width <<= 1
        return corner, width

    def make_tree(self, x):
        """Build tree"""
        cdef np.uint32_t i0 = 0
        cdef np.uint32_t i0max = x.shape[0]
        cdef int ndim = self.ndim
        cdef int clev, cell
        cdef np.uint64_t k
        self.keys = self.getkey(x)
        idx = np.argsort(self.keys)
        self.keys = self.keys[idx]
        self.icorner = self.icorner[idx]
        self.x = x[idx]
        if self.save_idx:
            self.idx = idx
        else:
            del idx
        self.tree = {}
        self.cell_maxn = 0
        self.cell_minn = i0max
        self.ncells = 0
        while i0 < i0max:
            i1 = i0 + self.ccut
            if i1 < i0max:
                clev = keylevel(self.keys[i1-1] - self.keys[i0]) - ndim
            else:
                clev = keylevel(self.hikey - self.keys[i0])
                i1 = i0max
            clev -= clev % ndim
            # don't go up over cell we just finished
            k = self.keys[i0]
            while (k >> clev) in self.tree:
                clev -= ndim
            cell = self.make_cell(i0, i1, clev)
            self.update_parents(cell, i0, clev)
            i0 += cell
        del self.keys
        del self.icorner
        print 'tree cell_minn: %d cell_maxn: %d avg: %.3f' % (self.cell_minn, self.cell_maxn, len(self.x)/self.ncells)
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
        cdef np.ndarray[np.float32_t, ndim=2] bounds = np.empty((3,2), dtype=np.float32)
        cdef int ncells = self.ncells
        cdef int cell_minn = self.cell_minn
        cdef int cell_maxn = self.cell_maxn
        cdef int cell = 0

        for i in range(i1-i0):
            cdx[i] = (keys[i0+i] >> clev) & 0x3f

        while cell < i1-i0 and cdx[cell] ^ cdx[0] < nsub:
            n = n_in_cell(cdx[cell:])
            k = keys[i0+cell] >> clev
            ii = self.icorner[i0+cell] & level_mask
            bounds[:,0] = self.bbox[:,0] + self.inv_keyfactor * ii
            bounds[:,1] = bounds[:,0] + self.cell_width[level]
            self.tree[k] = np.array((0, level, i0+cell, n, bounds), dtype=self.node)
            cell += n
            if n > cell_maxn: cell_maxn = n # keep some statistics
            if n < cell_minn: cell_minn = n
            ncells += 1
        self.ncells = ncells
        self.cell_minn = cell_minn
        self.cell_maxn = cell_maxn
        return cell

    def update_parents(self, int cell, np.uint32_t i0, int clev):
        """Create/update parent cells"""
        cdef int lev, level
        cdef np.uint64_t k, key
        cdef np.ndarray[np.uint32_t, ndim=1] ii = np.empty(self.ndim, np.uint32)
        cdef np.ndarray[np.uint64_t, ndim=1] keys = self.keys
        cdef np.ndarray[np.float32_t, ndim=2] bounds = np.empty((3,2), dtype=np.float32)
        key = keys[i0]
        for lev in range(clev+self.ndim, self.keybits+1, self.ndim):
            k = key >> lev
            if k in self.tree:
                self.tree[k]['len'] += cell
            else:
                level = self.bits_per_dim-lev/self.ndim
                ii = self.icorner[i0] & ~np.uint32((1 << lev/self.ndim) - 1)
                bounds[:,0] = self.bbox[:,0] + self.inv_keyfactor * ii
                bounds[:,1] = bounds[:,0] + self.cell_width[level]
                self.tree[k] = np.array((255, level, i0, cell, bounds), dtype=self.node)

    def find_nbrs(self, np.ndarray[np.float32_t, ndim=1] pos, float r):
        cdef int i
        cdef np.uint64_t k, cell
        cdef np.ndarray[np.uint64_t, ndim=1] nodes = np.array([8], dtype=np.uint64)
        cdef bbox = np.empty((3,2), np.float32)
        bbox[:,0] = pos-r
        bbox[:,1] = pos+r
        newnodes = []
        nbrlist = []
        while nodes.shape[0]:
            for cell in nodes:
                for i in range(8):
                    k = cell | i
                    if k not in self.tree: continue
                    abox = self.tree[k]['bbox']
                    if bbox_overlap(abox, bbox) and sphere_overlap(abox, pos, r):
                        if self.tree[k]['sbits']: # no daughters implies it is terminal
                            newnodes.append(k << 3)
                        else:
                            nbrlist.append(k)
            nodes = np.array(newnodes, dtype=np.uint64)
            newnodes = []
        return nbrlist

    def check_find(self, start=0):
        cdef int i
        cdef np.uint64_t k
        for i,x in enumerate(self.x[start:]):
            if (i % 1000) == 0: print i
            nbrs = self.find_nbrs(x, self.domain_width[0] * 1e-7)
            ok = False
            for k in nbrs:
                node = self.tree[k]
                for p in self.x[node['base']:node['base']+node['len']]:
                    if np.array_equal(p, x):
                        ok = True
                        break
            if not ok:
                raise 'ERROR, could not find x[%d]' % i


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

cdef inline sphere_overlap(float[:, ::1] a, float[::1] pos, float r):
    """Test if any part of box a is inside sphere"""
    cdef float w = 0.8660254 * (a[0,1] - a[0,0]) + r
    cdef float dr2 = 0.0
    cdef float dr
    cdef int i

    for i in range(a.shape[1]):
        dr = 0.5 * (a[i,0] + a[i,1]) - pos[i]
        dr2 += dr*dr

    return dr2 <= w*w

cdef inline bbox_overlap(float[:, ::1] a, float[:, ::1] b):
    """Test if any part of box a is inside box b"""
    cdef int i
    for i in range(3):
        if a[i,0] >= b[i,1] or a[i,1] < b[i,0]: return False
    return True
