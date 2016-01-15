#ifndef MUTATED_OPTS_HH
#define MUTATED_OPTS_HH

#include <cstdint>

/**
 * Command line parser for mutated.
 */
class Config
{
  public:
    char addr[128];    /* the server address */
    uint16_t port;     /* the server port */
    const char *label; /* label describing server (-m relevant only) */

    double service_us; /* service time mean microseconds */
    double req_s;      /* requests per second */

    uint64_t warmup_seconds;   /* number of seconds to warm up */
    uint64_t cooldown_seconds; /* number of seconds to cool down */
    uint64_t samples;          /* number of samples to measure */

    bool machine_readable; /* generate machine readable output? */

    enum conn_modes {
        PER_REQUEST,
        ROUND_ROBIN,
    };
    conn_modes conn_mode; /* the connection mode */
    uint64_t conn_cnt;    /* the number of connections to open */

    /* the remaining unparsed arguments */
    int gen_argc;
    char **gen_argv;

  public:
    Config(int argc, char *argv[]);
};

#endif /* MUTATED_OPTS_HH */
