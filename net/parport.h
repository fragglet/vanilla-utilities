// Declarations for the DOOM parallel port driver.
void ParallelRegisterFlags(void);
void InitPort(void);
void ShutdownPort(void);
unsigned int NextPacket(uint8_t *result_buf, unsigned int max_len);

