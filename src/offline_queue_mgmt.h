#ifndef __OFFLINE_QUEUE_MGMT__
#define __OFFLINE_QUEUE_MGMT__

enum
{
  MGMT_ADD,
  MGMT_DEL,
  MGMT_MOV,
  MGMT_REN,
  MGMT_GRP_ADD,
  MGMT_GRP_DEL,
  MGMT_GRP_REN
};

#ifdef __cplusplus
extern "C" {
#endif

void contact_mgmt_queue_add(const eb_account *ea, int action, const char *new_group);
void group_mgmt_queue_add(const eb_local_account *ela, const char *old_group, int action, const char *new_group);
int contact_mgmt_flush(eb_local_account *ela);
int group_mgmt_check_moved(const char *groupname);
int group_mgmt_flush(const eb_local_account *ela);

#ifdef __cplusplus
}
#endif


#endif
