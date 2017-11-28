
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void bt_agent_proxy_main_init();
void bt_agent_proxy_main_exit();
void *bt_agent_proxy_main_handle_msg(const void *msgv, size_t *replylen);
bool bt_agent_proxy_main_check_connect();

#ifdef __cplusplus
}
#endif /* __cplusplus */

