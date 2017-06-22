#define AGENT_COPYDATA_ID 0x804e50ba   /* random goop */
#define AGENT_MAX_MSGLEN  8192
/*
 * FIXME: maybe some day we can sort this out ...
 */
#define AGENT_MAX_MSGLEN  8192


/* Pageant client */
int agent_exists(void);
int agent_query(void *in, int inlen, void **out, int *outlen,
		void (*callback)(void *, void *, int), void *callback_ctx);

/*
 * pageantc.c needs to schedule callbacks for asynchronous agent
 * requests. This has to be done differently in GUI and console, so
 * there's an exported function used for the purpose.
 * 
 * Also, we supply FLAG_SYNCAGENT to force agent requests to be
 * synchronous in pscp and psftp.
 */
void agent_schedule_callback(void (*callback)(void *, void *, int),
			     void *callback_ctx, void *data, int len);
#define FLAG_SYNCAGENT 0x1000

