//
// P A R S E T U P . H
//
// Declarations for the DOOM parallel port driver.
//
//

typedef enum { false, true } boolean;

void ParallelRegisterFlags(void);
void InitPort(void);
void ShutdownPort(void);

