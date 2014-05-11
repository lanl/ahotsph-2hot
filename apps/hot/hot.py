import numpy as np
import gc

class HOT(object):
    """
    Hashed tree
    """

    def __init__(self,
                 bbox=np.array([[0.0]*3, [1.0]*3]).T):
        
        self.bbox = bbox
        self.ndim = np.shape(self.bbox)[0]
        self.placeholder = np.ones(1, dtype=np.uint64) # uint128 support would be nice
        self.bits_per_dim = self.placeholder.itemsize * 8 / self.ndim
        self.keybits = self.ndim * self.bits_per_dim
        self.placeholder <<= self.keybits
        self.domain_width = bbox[:,1] - bbox[:,0]
        self.domain_width *= 1.0 + 4.0 * np.finfo(bbox.dtype).eps
        self.keyfactor = (np.ones(1, dtype=np.uint64) << self.bits_per_dim) / self.domain_width
        self.lokey = self.placeholder
        self.hikey = self.placeholder | self.placeholder-1
        self.morton_table = np.zeros((256), dtype=np.uint32)
        for i in range(256):
            self.morton_table[i] = i&1 | (i>>1&1)<<3 | (i>>2&1)<<6 | (i>>3&1)<<9 | (i>>4&1)<<12 | (i>>5&1)<<15 | (i>>6&1)<<18 | (i>>7&1)<<21

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
    
    def sort(self, x):
        """Sort in Morton order"""
        return x[np.argsort(self.getkey(x))]

    def popcnt(self, key):
        """Number of 1 bits in key""" # Hamming weight
        return bin(key).count('1')

    def level(self, key):
        """keybits - clz(), tree root at zero"""
        lev = 0
        # numpy does not support shift on 64-bit scalar ints
        if key > np.uint64(0xffffffff): 
            key /= np.uint64(2**32)
            lev += 32
        if key > np.uint64(0xffff): 
            key /= np.uint64(2**16)
            lev += 16
        if key > np.uint64(0xff): 
            key /= np.uint64(2**8)
            lev += 8
        if key > np.uint64(0xf): 
            key /= np.uint64(2**4)
            lev += 4
        if key > np.uint64(0x3): 
            key /= np.uint64(2**2)
            lev += 2
        if key > np.uint64(0x1): 
            lev += 1
        return lev

    def keybits(self, x):
        """Print bitstring"""
        return [bin(k)[2:] for k in self.getkey(x)]

    def make_tree(self, x):
        """Build tree"""
        node = np.dtype({'names' : ['base', 'len'],
                       'formats' : [np.uint32, np.uint32]})
        keys = self.getkey(x)
        idx = np.argsort(keys)
        x = x[idx]
        keys = keys[idx]
        del idx
        gc.collect()
        i0 = 0
        i0max = len(x)
        chunk = 256
        tree = {}
        while i0 < i0max:
            i1 = i0 + chunk
            if i1 < i0max:
                clev = self.level(keys[i0] ^ keys[i1-1])
                clev -= clev % self.ndim + self.ndim
            else:
                # if last chunk, use old clev
                i1 = i0max
            cell = self.make_cell(i0, i1, clev, keys, tree, node)
            self.update_parents(cell, i0, clev, keys, tree, node)
            i0 += cell
        return tree

    def cellidx(self, i0, i1, clev, keys):
        return np.uint8((keys[i0:i1] >> clev) & 0x3f)

    def make_cell(self, i0, i1, clev, keys, tree, node):
        cell = 0
        k = self.cellidx(i0, i1, clev, keys)
        while cell < i1-i0 and k[cell] ^ k[0] < 1 << self.ndim:
            n = 0
            while cell+n < i1-i0 and k[cell] == k[cell+n]: n += 1
            tree[(keys[cell:cell+1] >> clev)[0]] = np.array((i0+cell, n), dtype=node)
            cell += n
        return cell

    def update_parents(self, cell, i0, clev, keys, tree, node):
        for lev in range(clev+self.ndim, self.keybits+1, self.ndim):
            key = (keys[cell:cell+1] >> lev)[0]
            if key in tree:
                tree[key]['len'] += cell
            else:
                tree[key] = np.array((i0, cell), dtype=node)
	

if __name__ == "__main__":
    ot = HOT()

    np.random.seed(0)
    x = ot.sort(np.random.rand(1e6,ot.ndim).astype(np.float32))

    tree = ot.make_tree(x)
    print tree[1]
