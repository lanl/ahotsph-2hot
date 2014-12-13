#define NDIM 3

typedef struct  {
    float mass;			/* mass of body */
    float pos[NDIM];		/* position of body */
    float vel[NDIM];		/* velocity of body */
    int64_t ident;		/* unique identifier */
} __attribute__ ((packed)) body;

typedef struct  {
#ifdef SAVE_MASS
    float mass;			/* mass of body */
#endif
    float pos[NDIM];		/* position of body */
    float vel[NDIM];		/* velocity of body */
#ifdef SAVE_ACC
    float acc[NDIM];
    float phi;
#endif
    int64_t ident;		/* unique identifier */
} __attribute__ ((packed)) outbody, *outbodyptr;

/* This is the descriptor that goes into the SDF header. */

#ifdef SAVE_ACC
#ifdef SAVE_MASS
#define OUTBODYDESC \
"struct {\n\
    float mass;			/* mass of body */\n\
    float x, y, z;		/* position of body */\n\
    float vx, vy, vz;		/* velocity of body */\n\
    float ax, ay, az;		/* acceleration */\n\
    float phi;			/* potential */\n\
    int64_t ident;		/* unique identifier */\n\
}"
#else
#define OUTBODYDESC \
"struct {\n\
    float x, y, z;		/* position of body */\n\
    float vx, vy, vz;		/* velocity of body */\n\
    float ax, ay, az;		/* acceleration */\n\
    float phi;			/* potential */\n\
    int64_t ident;		/* unique identifier */\n\
}"
#endif /* SAVE_MASS */
#else
#ifdef SAVE_MASS
#define OUTBODYDESC \
"struct {\n\
    float mass;			/* mass of body */\n\
    float x, y, z;		/* position of body */\n\
    float vx, vy, vz;		/* velocity of body */\n\
    int64_t ident;		/* unique identifier */\n\
}"
#else
#define OUTBODYDESC \
"struct {\n\
    float x, y, z;		/* position of body */\n\
    float vx, vy, vz;		/* velocity of body */\n\
    int64_t ident;		/* unique identifier */\n\
}"
#endif /* SAVE_MASS */
#endif /* SAVE_ACC */

