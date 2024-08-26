/* #define NO_MSGS */
#include "Assert.h"
#include "Malloc.h"
#include "Msgs.h"
#include "abm.h"
#include "chn.h"
#include "error.h"
#include "key.h"
#include "mpmy.h"
#include "stk.h"
#include "timers.h"
#include "tree.h"

/* Emit a warning if we Poll this many times without making any progress */
#define NOPROGRESS 100000

/* Make the packet size a couple of times bigger than the largest possible
   reply.  Don't sweat the details.  Just make it nice and big. */
#define ABMPKTSIZE 16384
Counter_t DeferCnt;
Counter_t RequestCnt;
Timer_t WalkDeferTm;

/* Should there be a data structure with all this in it??? */
/* Should that data structure be tree_t?? */
static int level;
static int common_level;
static int done_first;
static Stk *unacc;
static Stk *unacc_flags;
static void **sink_tbl;
static Stk def1, def2;
static Stk def1_flags, def2_flags;
static Stk arrived;
static Stk arrived_flags;
static Stk walkstk1, walkstk2;
static Stk flags1, flags2;
static hcell **hc_tbl;
static int max_pp_vec;
static int *result_vec;
static hcell **pp_vec;
static tree_t *Srctp, *Sinktp;
static walkinit_t Init;
static macv_t MACv;
static inherit_t Inherit;
static float Sinksz;

static ABM Abm;
static int Abm_active;
static ABMhndlr_t reqhndlr;
static ABMhndlr_t replyhnlr;

/* These correspond to entries in walkhndlrs.
   A REQUESTTYPE will invoke walkhndlr[REQUESTTYPE] (i.e., reqhndlr).
   A REPLYTYPE will invoke walkhndlr[REPLYTYPE] (i.e., replyhnlr)
   */
#define REQUESTTYPE 0
#define REPLYTYPE 1
/* Some compilers can't handle this initiaizer.  I'll put it in
   setup for their benefit.  Who knows, maybe it's really non-ANSI... */
static ABMhndlr_t *walkhndlrs[2]; /* = {&reqhndlr, &replyhnlr}; */

/* The tag to use for all asynchronous walk messages. */
#define WALKTAG 3210
static ABMpktz_t copyContents;
static int szContents(hcell *pp);


#define ABMPollMaybe(abmp, nskip)          \
    do {                                   \
        static int defer_cnt = 0;          \
        if (defer_cnt-- <= 0) {            \
            defer_cnt = (nskip);           \
            if (ABMPoll(abmp) < 0)         \
                Error("ABMPoll failed\n"); \
        }                                  \
    } while (0)

static void setupWalk(
    tree_t *srctp, tree_t *sinktp, int sinksz, walkinit_t init, inherit_t inherit, macv_t macV) {
    int i, chubits2;

    level = 0;
    common_level = 0;
    done_first = 0;
    StkInitEz(&def1);
    StkInitEz(&def1_flags);
    StkInitEz(&def2);
    StkInitEz(&def2_flags);
    StkInitEz(&arrived);
    StkInitEz(&arrived_flags);
    StkInitEz(&walkstk1);
    StkInitEz(&flags1);
    StkInitEz(&walkstk2);
    StkInitEz(&flags2);
    chubits2 = ((KEYBITS - 1) / (sinktp->ndim)) + 2;
    sink_tbl = Malloc(chubits2 * sizeof(void *));
    hc_tbl = Malloc(chubits2 * sizeof(hcell *));
    unacc = Malloc(chubits2 * sizeof(Stk));
    unacc_flags = Malloc(chubits2 * sizeof(Stk));

    sink_tbl[0] = Malloc(chubits2 * sinksz);
    inherit(NULL, sink_tbl[0], Find(sinktp, KeyInt(1)));
    for (i = 0; i < chubits2; i++) {
        if (i > 0) {
            sink_tbl[i] = sinksz + (char *)(sink_tbl[i - 1]);
        }
        StkInitEz(&unacc[i]);
        StkInitEz(&unacc_flags[i]);
        hc_tbl[i] = NULL;
    }
    init(&unacc[0], &unacc_flags[0]);
    max_pp_vec = 0;
    result_vec = NULL;
    pp_vec = NULL;
}

static void terminateWalk(void) {
    int chubits2, i;

    StkTerminate(&def1);
    StkTerminate(&def1_flags);
    StkTerminate(&def2);
    StkTerminate(&def2_flags);
    StkTerminate(&arrived);
    StkTerminate(&arrived_flags);
    StkTerminate(&walkstk1);
    StkTerminate(&flags1);
    StkTerminate(&walkstk2);
    StkTerminate(&flags2);
    chubits2 = ((KEYBITS - 1) / (Sinktp->ndim)) + 2;
    for (i = 0; i < chubits2; i++) {
        StkTerminate(&unacc[i]);
        StkTerminate(&unacc_flags[i]);
    }
    Free(pp_vec);
    Free(result_vec);
    Free(sink_tbl[0]);
    Free(sink_tbl);
    Free(hc_tbl);
    Free(unacc);
    Free(unacc_flags);
    pp_vec = NULL;
    result_vec = NULL;
    sink_tbl = NULL;
    hc_tbl = NULL;
    unacc = NULL;
    unacc_flags = NULL;
    max_pp_vec = 0;
}

void findv(const tree_t *__restrict__ tp,
           const Key_t *__restrict__ key_vec,
           hcellptr *__restrict__ out,
           const int n) {
    hcellptr *firstp, *prevnext, np;

    for (int i = 0; i < n; i++) {
        firstp = tp->htab + KeyAndInt(key_vec[i], tp->hash_mask);

        for (np = *(prevnext = firstp);
#if NK == 1
             np && (np->key.k[0] != key_vec[i].k[0]);
#else
             np && (np->key.k[0] != key_vec[i].k[0] || np->key.k[1] != key_vec[i].k[1]);
#endif
             np = *(prevnext = &np->next)) {
        }

        out[i] = np;

        if (np) {
            *prevnext = np->next; /* this shouldn't happen if np was at top */
            np->next = *firstp;
            *firstp = np;
        }
    }
}

/* flags are for things like periodic boundary conditions, where we alias the */
/* same cell to multiple spatial locations */

static void walkv(void *sink,
                  const Stk *parent_unacc,
                  const Stk *parent_flags,
                  Stk *unacceptable,
                  Stk *unacceptable_flags,
                  Stk *deferred,
                  Stk *deferred_flags) {
    Key_t key;
    hcell_type type;
    int n;
    const Stk *in, *in_flags;
    Stk *nextin, *nextin_flags;
    const Key_t *key_vec;
    int *flags_vec;
    int i, nvec;
    int toggle;
    int ndim = Srctp->ndim;

    in = parent_unacc;
    in_flags = parent_flags;
    nextin = &walkstk1;
    nextin_flags = &flags1;
    toggle = 0;
    while ((nvec = StkSz(in) / sizeof(Key_t)) > 0) {
        ABMPollMaybe(&Abm, 4);
        if (nvec > max_pp_vec) {
            max_pp_vec = nvec + 512;
            pp_vec = Realloc(pp_vec, max_pp_vec * sizeof(hcellptr));
            result_vec = Realloc(result_vec, max_pp_vec * sizeof(int));
        }
        StkClear(nextin);
        StkClear(nextin_flags);
        key_vec = StkBase(in);
        flags_vec = Malloc(StkSz(in_flags));
        memcpy(flags_vec, StkBase(in_flags), StkSz(in_flags));
        findv(Srctp, key_vec, pp_vec, nvec);
        MACv(sink, (const hcell **)pp_vec, flags_vec, result_vec, nvec);

        for (i = 0; i < nvec; i++) {
            key = *(Key_t *)((char *)key_vec + i * StkAlign(in, sizeof(Key_t)));
            switch (result_vec[i]) {
                case MAC_SPLIT_SINK:
                    StkPushType(unacceptable, key, Key_t);
                    StkPushType(unacceptable_flags, flags_vec[i], int);
                    break;
                case MAC_SPLIT_SRC:
                    type = Type(pp_vec[i]);

                    if (TreeKidsOK(type)) {
                        key = KeyLshift(key, ndim);
                        for (n = Sub_Flags_Type(type); n; n >>= 1, key.k[0]++) {
                            if ((n & 1) == 0)
                                continue;
                            StkPushType(nextin, key, Key_t);
                            StkPushType(nextin_flags, flags_vec[i], int);
                        }
                    } else {
                        StkPushType(deferred, key, Key_t);
                        StkPushType(deferred_flags, flags_vec[i], int);
                        IncrCounter(&DeferCnt);
                        if (DeferCnt.counter > 3000000) {
                            SeriousWarning("Defer count is very large\n");
                            DeferCnt.counter = 0;
                        }
                        if ((type & REQUESTED) == 0) {
                            Type(pp_vec[i]) |= REQUESTED;
                            IncrCounter(&RequestCnt);
                            Msgf((
                                "Request %s from %d\n", PrintKey(pp_vec[i]->key), GetSource(type)));
                            ABMPost(&Abm,
                                    GetSource(type),
                                    sizeof(Key_t),
                                    REQUESTTYPE,
                                    (ABMpktz_t *)&memcpy,
                                    &(pp_vec[i]->key));
                        }
                    }
                    break;
                case MAC_ACCEPT:
                    break;
                case MAC_ERROR:
                    Error("MAC returned MAC_ERROR\n");
                default:
                    Error("Unknown MAC result in walkv\n");
            }
        }
        Free(flags_vec);
        /* Toggle in and nextin */
        if (toggle) {
            in = &walkstk2;
            in_flags = &flags2;
            nextin = &walkstk1;
            nextin_flags = &flags1;
            toggle = 0;
        } else {
            in = &walkstk1;
            in_flags = &flags1;
            nextin = &walkstk2;
            nextin_flags = &flags2;
            toggle = 1;
        }
    }
    ABMFlush(&Abm);
}

/* This version does deferrals only for the duration of the MACv. */
/* It might be worthwhile to defer for longer by keeping separate defer */
/* lists at each level and putting another loop around the whole thing. */
static void Walkbody(int commonlev, int bodylev) {
    int l;
    Stk *deffrom, *deffrom_flags, *defto, *defto_flags, *deftmp;
    void *sink;
    int ndef, i;
    Key_t *key_vec;
    int *flags_vec;
    int foundone;

    for (l = commonlev; l < bodylev; l++) {
        ABMPoll(&Abm);
        Inherit(sink_tbl[l], sink_tbl[l + 1], hc_tbl[l + 1]);
        sink = sink_tbl[l + 1];
        StkClear(&def1);
        StkClear(&def1_flags);
        StkClear(&unacc[l + 1]);
        StkClear(&unacc_flags[l + 1]);
        ABMPoll(&Abm);
        walkv(sink,
              &unacc[l],
              &unacc_flags[l],
              &unacc[l + 1],
              &unacc_flags[l + 1],
              &def1,
              &def1_flags);
        deffrom = &def1;
        deffrom_flags = &def1_flags;
        defto = &def2;
        defto_flags = &def2_flags;
        foundone = 1; /* ???? could be 0 the first time ???  */
        while (StkSz(deffrom)) {
            StkClear(defto);
            StkClear(defto_flags);
            ABMFlush(&Abm); /* every time we are deferred */
            /* We can't get out of here until something arrives, so
               we might be better off calling PollWait, which >>might<<
               be more sociable about letting somebody else have the CPU */
            /* But we shouldn't wait if some new stuff has arrived since
               last time because there might be oodles of good stuff
               available in memory to chew on... */
            if (!foundone) {
                StartTimer(&WalkDeferTm);
                if (ABMPollWait(&Abm) < 0)
                    Error("ABMPollWait failed\n");
                StopTimer(&WalkDeferTm);
            } else {
                ABMPollMaybe(&Abm, 10);
            }
            /* This code is almost identical to the code in walkv...*/
            ndef = StkSz(deffrom) / sizeof(Key_t);
            key_vec = StkBase(deffrom);
            flags_vec = StkBase(deffrom_flags);
            if (ndef > max_pp_vec) {
                max_pp_vec = ndef + 512;
                pp_vec = Realloc(pp_vec, max_pp_vec * sizeof(hcellptr));
                result_vec = Realloc(result_vec, max_pp_vec * sizeof(int));
            }
            findv(Srctp, key_vec, pp_vec, ndef);
            StkClear(&arrived);
            StkClear(&arrived_flags);
            foundone = 0;
            for (i = 0; i < ndef; i++) {
                int type;
                Key_t key;
                int ndim = Srctp->ndim;
                type = Type(pp_vec[i]);
                /* key = key_vec[i] */
                key = *(Key_t *)((char *)key_vec + i * StkAlign(deffrom, sizeof(Key_t)));
                if (TreeKidsOK(type)) {
                    int n;
                    foundone = 1;
                    key = KeyLshift(key, ndim);
                    for (n = Sub_Flags_Type(type); n; n >>= 1, key.k[0]++) {
                        if ((n & 1) == 0)
                            continue;
                        StkPushType(&arrived, key, Key_t);
                        StkPushType(&arrived_flags, flags_vec[i], int);
                    }
                } else {
                    StkPushType(defto, key, Key_t);
                    StkPushType(defto_flags, flags_vec[i], int);
                    IncrCounter(&DeferCnt);
                    if (DeferCnt.counter > 3000000) {
                        SeriousWarning("Defer count is very large\n");
                        DeferCnt.counter = 0;
                    }
                    assert(type & REQUESTED);
                }
            }
            walkv(sink,
                  &arrived,
                  &arrived_flags,
                  &unacc[l + 1],
                  &unacc_flags[l + 1],
                  defto,
                  defto_flags);
            deftmp = deffrom;
            deffrom = defto;
            defto = deftmp;
            deftmp = deffrom_flags;
            deffrom_flags = defto_flags;
            defto_flags = deftmp;
        }
    }
    if (TreeLocal(hc_tbl[bodylev]->type))
        Inherit(sink_tbl[bodylev], NULL, hc_tbl[bodylev]);
    ABMFlush(&Abm);
}

static int preWalk(tree_t *tp, hcell *hp) {
    hc_tbl[level + 1] = hp;
    if (!TreeLocal(hp->type))
        return 0;
    if (Sub_Flags(hp) == 0) {
        if (tp->BodyActive == NULL || tp->BodyActive(hp->ptr)) {
            if (Msg_test(__FILE__)) {
                Msg_do("Body at %s.\n", hcellPrint(hp));
                Msg_do("level=%d, common level=%d\n", level + 1, common_level);
            }
            Walkbody(done_first ? common_level : 0, level + 1);
            done_first = 1;
            common_level = level;
        }
    } else if (tp->CellActive == NULL || tp->CellActive(hp->ptr)) {
        level++;
        return 1;
    }
    return 0;
}

static void postWalk(tree_t *tp, hcell *hp, hcell **daughters) { common_level = --level; }


void Walk(tree_t *srctp,
          tree_t *sinktp,
          int sinksz,
          walkinit_t WalkInitSrc,
          macv_t MAC,
          inherit_t InheritSink) {
    WalkInit(srctp, sinktp, sinksz, WalkInitSrc, MAC, InheritSink);
    WalkNT(sinktp);
    WalkTerminate();
}

/* A way to let application code yield to ABM() */
void WalkPoll(void) {
    if (Abm_active) {
        ABMPoll(&Abm);
        ABMFlush(&Abm);
    }
}


void WalkInit(tree_t *srctp,
              tree_t *sinktp,
              int sinksz,
              walkinit_t WalkInit,
              macv_t MAC,
              inherit_t InheritSink) {
    walkhndlrs[REQUESTTYPE] = reqhndlr;
    walkhndlrs[REPLYTYPE] = replyhnlr;
    Msgf(("ABMsetup pktsize %d\n", ABMPKTSIZE));
    ABMSetup(&Abm, ABMPKTSIZE, WALKTAG, 2, walkhndlrs);
    Abm_active = 1;
    Srctp = srctp;
    MACv = MAC;
    Init = WalkInit;
    Inherit = InheritSink;
    Sinktp = sinktp;
    Sinksz = sinksz;
}

void WalkNT(tree_t *sinktp) {
    setupWalk(Srctp, Sinktp, Sinksz, Init, Inherit, MACv);
    Traverse(sinktp, Find(sinktp, KeyInt(1)), preWalk, postWalk);
    terminateWalk();
}

void WalkTerminate(void) {
    Abm_active = 0;
    ABMIamDone(&Abm);
    while (!ABMAllDone(&Abm)) {
        if (ABMPoll(&Abm) < 0)
            Error("ABMPoll fails while waiting for Alldone\n");
        ABMFlush(&Abm);
    }
    ABMShutdown(&Abm);
    Srctp = NULL;
    MACv = NULL;
}

/* How big will the message be that returns the "Contents" of pp? */
static int szContents(hcell *pp) {
    unsigned int sub_flags, n;
    Key_t key, key0;
    int ndim = Srctp->ndim;
    int answer;

    answer = sizeof(Key_t);
    sub_flags = Sub_Flags(pp);
    key0 = KeyLshift(pp->key, ndim);
    answer += sizeof(int);
    for (n = 0; sub_flags; n++, sub_flags >>= 1) {
        if ((sub_flags & 1) == 0)
            continue;
        key = KeyOrInt(key0, n);
        pp = Find(Srctp, key);
        assert(pp);
        answer += sizeof(int); /* 'type' */
        if (Sub_Flags(pp))
            answer += Srctp->CellSz(pp->ptr);
        else
            answer += Srctp->tbody_sz;
    }
    /* Msgf(("szContents(%s)=%d\n", PrintKey(KeyRshift(pp->key,ndim)), answer)); */
    return answer;
}

static void copyContents(void *to, void *vp, int sz) {
    hcell *pp = vp;
    unsigned int sub_flags, n;
    Key_t key, key0;
    int ndim = Srctp->ndim;
    int type;
    char *p = to;

    /* Msgf(("copyContents %s %d\n", PrintKey(pp->key), sz)); */

#if FORCE_KEY_ALIGNMENT
    memcpy(p, &pp->key, sizeof(Key_t));
#else
    *(Key_t *)p = pp->key;
#endif
    p += sizeof(Key_t);
    sub_flags = Sub_Flags(pp);
    key0 = KeyLshift(pp->key, ndim);
    *(int *)p = sub_flags;
    p += sizeof(int);
    for (n = 0; sub_flags; n++, sub_flags >>= 1) {
        if ((sub_flags & 1) == 0)
            continue;
        key = KeyOrInt(key0, n);
        pp = Find(Srctp, key);
        assert(pp);
        type = (pp->type & ~KIDSHERE & ~REQUESTED) | DATAHERE | NONLOCAL;
        if (GetSource(pp->type) == -1)
            type |= PutSource(MPMY_Procnum());
        *(int *)p = type;
        p += sizeof(type);
        if (Sub_Flags(pp)) {
            int size = Srctp->CellSz(pp->ptr);
            memcpy(p, pp->ptr, size);
            p += size;
        } else {
            memcpy(p, pp->ptr, Srctp->tbody_sz);
            p += Srctp->tbody_sz;
        }
    }
    assert((p - (char *)to) == sz);
}

static void reqhndlr(int src, int sz, void *p) {
    Key_t key;
    hcell *pp;
    int repsz;

#if FORCE_KEY_ALIGNMENT
    memcpy(&key, p, sizeof(Key_t));
#else
    key = *(Key_t *)p;
#endif
    Msgf(("WalkReply p%d %d@%p %s\n", src, sz, p, PrintKey(key)));
    /* We have to Find it in order to get the size.  That's too bad because
       otherwise we could postpone the whole thing until we get called-back
       via copyContents */
    pp = Find(Srctp, key);
    assert(pp);
    repsz = szContents(pp);
    ABMPost(&Abm, src, repsz, REPLYTYPE, copyContents, pp);
}

static void replyhnlr(int src, int sz, void *vp) {
    char *p = vp; /* so we can do arithmetic */
    Key_t key0;
    Key_t key;
    unsigned int n, sub_flags;
    hcell *parent;
    int type;
    void *c;

#if FORCE_KEY_ALIGNMENT
    memcpy(&key0, p, sizeof(Key_t));
#else
    key0 = *(Key_t *)p;
#endif
    Msgf(("WalkReact to %s from p%d\n", PrintKey(key0), src));
    p += sizeof(Key_t);
    parent = Find(Srctp, key0);

    sub_flags = *(int *)p;
    p += sizeof(int);
    Set_Sub_Flag(parent, sub_flags);
    key0 = KeyLshift(key0, Srctp->ndim);

    for (n = 0; sub_flags; n++, sub_flags >>= 1) {
        if ((sub_flags & 1) == 0)
            continue;
        key = KeyOrInt(key0, n);
        type = *(int *)p;
        p += sizeof(int);
        if (Sub_Flags_Type(type)) {
            int size = Srctp->CellSz(p);
            if (size == Srctp->cell_sz) {
                c = ChnAlloc(&Srctp->cellchn);
                Msgf(("c%d ", n));
            } else if (size == Srctp->cell2_sz) {
                c = ChnAlloc(&Srctp->cell2chn);
                Msgf(("p2%d ", n));
            } else if (size == Srctp->cell4_sz) {
                c = ChnAlloc(&Srctp->cell4chn);
                Msgf(("p4%d ", n));
            } else if (size == Srctp->cell8_sz) {
                c = ChnAlloc(&Srctp->cell8chn);
                Msgf(("p8%d ", n));
            } else {
                Error("Bad CellSz in replyhnlr (%d)\n", size);
            }
            memcpy(c, p, size);
            p += size;
        } else {
            c = ChnAlloc(&Srctp->tbodychn);
            Msgf(("t%d ", n));
            memcpy(c, p, Srctp->tbody_sz);
            p += Srctp->tbody_sz;
        }
        Enter(Srctp, key, c, type);
    }
    Msgf(("\n"));
    parent->type |= KIDSHERE;
    parent->type &= ~REQUESTED;
}
