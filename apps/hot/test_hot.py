import numpy as np
import chot

if __name__ == "__main__":
    from time import *

    npart = 1e7
    ot = chot.HOT()

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
