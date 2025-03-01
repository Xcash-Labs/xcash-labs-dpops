#ifndef NETWORK_SECURITY_FUNCTIONS_H_   /* Include guard */
#define NETWORK_SECURITY_FUNCTIONS_H_

int sign_data(char *message);
int validate_data(const char* MESSAGE);
int verify_data(const char* MESSAGE, const int VERIFY_CURRENT_ROUND_PART_BACKUP_NODE_SETTINGS);
#endif