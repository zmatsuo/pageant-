

typedef struct agent_pending_query agent_pending_query;
agent_pending_query *agent_query(
    void *in, int inlen, void **out, int *outlen,
    void (*callback)(void *, void *, int), void *callback_ctx);
void agent_cancel_query(agent_pending_query *);
void agent_query_synchronous(void *in, int inlen, void **out, int *outlen);
int agent_exists(void);
