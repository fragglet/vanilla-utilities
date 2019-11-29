//
// P A R S E T U P . H
//
// Declarations for the DOOM parallel port driver.
//
//

typedef unsigned char byte;
typedef enum { false, true } boolean;

void ParallelRegisterFlags(void);
void InitPort(void);
void ShutdownPort(void);

byte readport(void);
int getb(void);
void putb(byte b);

void Error(char *error, ...);
