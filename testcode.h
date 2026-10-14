struct testinfo
{
    char            errors[128];/* accumulated errors as a string */
    char            error,	/* error flag */
                    tested;
    const char     *stage;      /* current target payload */
    const char     *skipped;    /* omitted payload, if any */
};

#ifdef __STDC__
int load_failed(struct tpstats *p);
int  getresult(LINK TheLink, struct tpstats *p, char logging);
int testT2    (LINK TheLink, struct tpstats *p, char logging);
int testT414  (LINK TheLink, struct tpstats *p, char logging);
int testT425  (LINK TheLink, struct tpstats *p, char logging);
int testT32   (LINK TheLink, struct tpstats *p, char logging);
int testT80x  (LINK TheLink, struct tpstats *p, char logging);
int testT800  (LINK TheLink, struct tpstats *p, char logging);
int testT801  (LINK TheLink, struct tpstats *p, char logging);
int later_supported(int type);
int testLater(LINK link, struct tpstats *p, char logging, int probe);
#else
int load_failed();
int  getresult();
int testT2    ();
int testT414  ();
int testT425  ();
int testT32   ();
int testT80x  ();
int testT800  ();
int testT801  ();
int later_supported();
int testLater();
#endif
