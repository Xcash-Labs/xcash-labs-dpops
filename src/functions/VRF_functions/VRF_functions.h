#ifndef VRF_FUNCTIONS_H_   /* Include guard */
#define VRF_FUNCTIONS_H_

void generate_key(void);
int VRF_sign_data(char *beta_string, char *proof, const char* data);

#endif